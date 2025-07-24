#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <utility>
#include <vector>
#include <type_traits>
#include <complex>
#include <Eigen/Core>

#include "basesolver.hpp"
#include "indata.hpp"
#include "linalg.hpp"
#include "forwardDecl.hpp"

void BaseSolver::loadMaterialData()
{
  // Logging
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "                      Loading material data                      \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";

  matstack.numLayers = static_cast<Eigen::Index>(layers.size());
  matstack.numInterfaces = matstack.numLayers - 1;
  matstack.numLayersTop = dipoleLayer + 1;
  matstack.numLayersBottom = matstack.numLayers - dipoleLayer;

  matstack.epsilon.resize(matstack.numLayers);
  for (size_t i = 0; i < static_cast<size_t>(matstack.numLayers); ++i) {
    matstack.epsilon(i) = layers[i].getMaterial().getEpsilon(wvl);
    std::cout << "Layer " << i << "; Material: (" << matstack.epsilon(i).real() << ", " << matstack.epsilon(i).imag()
              << ")\n";
  }
}

BaseSolver::BaseSolver(const std::vector<Layer>& layers,
  const double dipolePosition,
  Spectrum<Distribution> spectrum,
  const double sweepStart,
  const double sweepStop,
  const double inpalpha) :
  layers{std::move(layers)},
  dipolePosition{dipolePosition},
  _sweepStart{sweepStart},
  _sweepStop{sweepStop},
  alpha{inpalpha}

{
  dipoleLayer = 0;
  for (auto layer : layers) {
    if (layer.isEmitter) { break; }
    dipoleLayer++;
  }
  _spectrum = std::move(spectrum.spectrum);
  if (_spectrum.rows() == 1) wvl = _spectrum(0, 0);
  _dipolePositions = Vector::Zero(10);

  resMap["pPerpPSub"] = Vector();
  resMap["pParaPSub"] = Vector();
  resMap["pParaSSub"] = Vector();

  resMap["pPerpP"] = CMatrix();
  resMap["pParaP"] = CMatrix();
  resMap["pParaS"] = CMatrix();

  resMap["fpPerpP"] = Matrix();
  resMap["fpParaP"] = Matrix();
  resMap["fpParaS"] = Matrix();
}

BaseSolver::BaseSolver(const std::vector<Layer>& layers,
  const Distribution& dipoleDist,
  Spectrum<Distribution> spectrum,
  const double sweepStart,
  const double sweepStop,
  const double inpalpha) :
  BaseSolver(layers, 0.0, spectrum, sweepStart, sweepStop, inpalpha)
{
  _dipolePositions = std::move(dipoleDist.values);
}

void BaseSolver::calculateFresnelCoeffs()
{
  CMatrix R_perp(matstack.numInterfaces, matstack.numKVectors);
  CMatrix R_para(matstack.numInterfaces, matstack.numKVectors);

  R_perp = (matstack.h.block(1, 0, matstack.h.rows() - 1, matstack.h.cols()) -
             matstack.h.block(0, 0, matstack.h.rows() - 1, matstack.h.cols())) /
           (matstack.h.block(1, 0, matstack.h.rows() - 1, matstack.h.cols()) +
             matstack.h.block(0, 0, matstack.h.rows() - 1, matstack.h.cols()));
  (R_perp.bottomRows(R_perp.rows() - dipoleLayer)) *= -1.0;

  R_para = ((matstack.h.block(0, 0, matstack.h.rows() - 1, matstack.h.cols())).colwise() *
              matstack.epsilon.segment(1, matstack.epsilon.size() - 1) -
            (matstack.h.block(1, 0, matstack.h.rows() - 1, matstack.h.cols())).colwise() *
              matstack.epsilon.segment(0, matstack.epsilon.size() - 1));

  R_para /= ((matstack.h.block(0, 0, matstack.h.rows() - 1, matstack.h.cols())).colwise() *
               matstack.epsilon.segment(1, matstack.epsilon.size() - 1) +
             (matstack.h.block(1, 0, matstack.h.rows() - 1, matstack.h.cols())).colwise() *
               matstack.epsilon.segment(0, matstack.epsilon.size() - 1));

  (R_para.bottomRows(R_para.rows() - dipoleLayer)) *= -1.0;
  // Move into SolverCoefficients
  coeffs._Rperp = std::move(R_perp);
  coeffs._Rpara = std::move(R_para);
}

void BaseSolver::calculateGFCoeffRatios()
{
  CMPLX I(0.0, 1.0);
  CMatrix CB, FB, CT, FT;
  CB = FB = CMatrix::Zero(matstack.numLayersTop, matstack.numKVectors);
  CT = FT = CMatrix::Zero(matstack.numLayersBottom, matstack.numKVectors);

  for (Eigen::Index i = 1; i < dipoleLayer + 1; ++i) {
    CTVector neg_exp = Eigen::exp(-2.0 * I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1));
    CTVector pos_exp = Eigen::exp(2.0 * I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1));

    CB.row(i) = neg_exp * (coeffs._Rperp.row(i - 1) + (CB.row(i - 1) * pos_exp)) / 
                (1 + coeffs._Rperp.row(i - 1) * (CB.row(i - 1) * pos_exp));

    FB.row(i) = neg_exp * (-coeffs._Rpara.row(i - 1) + (FB.row(i - 1) * pos_exp)) / 
                (1 - coeffs._Rpara.row(i - 1) * (FB.row(i - 1) * pos_exp));
  }

  for (Eigen::Index i = matstack.numLayers - dipoleLayer - 2; i >= 0; --i) {
    Eigen::Index indexFromTop = i + dipoleLayer;
    CTVector neg_exp = Eigen::exp(-2.0 * I * matstack.h.row(indexFromTop + 1) * (matstack.z0.cast<CMPLX>())(indexFromTop));
    CTVector pos_exp = Eigen::exp(2.0 * I * matstack.h.row(indexFromTop) * (matstack.z0.cast<CMPLX>())(indexFromTop));

    CT.row(i) = pos_exp * (coeffs._Rperp.row(indexFromTop) + (CT.row(i + 1) * neg_exp)) / 
                (1 + coeffs._Rperp.row(indexFromTop) * (CT.row(i + 1) * neg_exp));
    FT.row(i) = pos_exp * (-coeffs._Rpara.row(indexFromTop) + (FT.row(i + 1) * neg_exp)) /
                (1 - coeffs._Rpara.row(indexFromTop) * (FT.row(i + 1) * neg_exp));
  }
  // Move results into SolverCoefficients
  coeffs._cb = std::move(CB);
  coeffs._fb = std::move(FB);
  coeffs._ct = std::move(CT);
  coeffs._ft = std::move(FT);
}

void BaseSolver::calculateGFCoeffs()
{

  CMPLX I(0.0, 1.0);
  CMatrix c, cd, f_perp, fd_perp, f_para, fd_para;
  c = cd = f_perp = fd_perp = f_para = fd_para = CMatrix::Zero(matstack.numLayers, matstack.numKVectors);

  c.row(dipoleLayer) =
    (coeffs._cb.row(dipoleLayer) + 1) * coeffs._ct.row(0) / (1 - coeffs._cb.row(dipoleLayer) * coeffs._ct.row(0));
  cd.row(dipoleLayer) =
    (coeffs._ct.row(0) + 1) * coeffs._cb.row(dipoleLayer) / (1 - coeffs._cb.row(dipoleLayer) * coeffs._ct.row(0));

  f_perp.row(dipoleLayer) =
    (coeffs._fb.row(dipoleLayer) + 1) * coeffs._ft.row(0) / (1 - coeffs._fb.row(dipoleLayer) * coeffs._ft.row(0));
  fd_perp.row(dipoleLayer) =
    (coeffs._ft.row(0) + 1) * coeffs._fb.row(dipoleLayer) / (1 - coeffs._fb.row(dipoleLayer) * coeffs._ft.row(0));

  f_para.row(dipoleLayer) =
    (coeffs._fb.row(dipoleLayer) - 1) * coeffs._ft.row(0) / (1 - coeffs._fb.row(dipoleLayer) * coeffs._ft.row(0));
  fd_para.row(dipoleLayer) =
    (1 - coeffs._ft.row(0)) * coeffs._fb.row(dipoleLayer) / (1 - coeffs._fb.row(dipoleLayer) * coeffs._ft.row(0));

  Vector boolValue = Vector::Zero(matstack.numLayers);
  boolValue(dipoleLayer) = 1.0;

  for (Eigen::Index i = dipoleLayer; i >= 1; --i) {

    CTVector h_frac = matstack.h.row(i) / matstack.h.row(i - 1);
    CTVector c_neg_exp = Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1));
    CTVector c_pos_exp = Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1));

    CTVector neg_exp = 0.5 * Eigen::exp(-I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1));
    CTVector pos_exp = 0.5 * Eigen::exp(I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1));

    CMPLX k_frac = matstack.k(i) / matstack.k(i - 1); 
    CTVector f_fact = h_frac/k_frac;
    
    c.row(i - 1) = pos_exp * ((boolValue(i) + c.row(i)) * c_neg_exp * (1 + h_frac) +
                   cd.row(i) * c_pos_exp * (1 - h_frac));

    cd.row(i - 1) = neg_exp * ((boolValue(i) + c.row(i)) * c_neg_exp * (1 - h_frac) +
                    cd.row(i) * c_pos_exp * (1 + h_frac));

    f_perp.row(i - 1) = pos_exp * ((boolValue(i) + f_perp.row(i)) * neg_exp * (k_frac + f_fact) +
                        fd_perp.row(i) * pos_exp * (k_frac - f_fact));

    fd_perp.row(i - 1) = neg_exp * ((boolValue(i) + f_perp.row(i)) * neg_exp * (k_frac - f_fact) +
                         fd_perp.row(i) * pos_exp * (k_frac + f_fact));

    f_para.row(i - 1) = pos_exp * ((boolValue(i) + f_para.row(i)) * neg_exp * (k_frac + f_fact) +
                         fd_para.row(i) * pos_exp * (k_frac - f_fact));

    fd_para.row(i - 1) = neg_exp * ((boolValue(i) + f_para.row(i)) * neg_exp * (k_frac - f_fact) +
                         fd_para.row(i) * pos_exp * (k_frac + f_fact));
  }

  for (Eigen::Index i = dipoleLayer; i < matstack.numLayers - 1; ++i) {

    //THE FOLLOWING DEFINITIONS ARE SLIGHTLY DIFFERENT FROM BEFORE DUE TO THE LOOP'S DIRECTION
    CTVector h_frac = matstack.h.row(i) / matstack.h.row(i + 1);

    CTVector c_neg_exp = Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i));
    CTVector c_pos_exp = Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i));

    CTVector neg_exp = 0.5 * Eigen::exp(-I * matstack.h.row(i + 1) * (matstack.z0.cast<CMPLX>())(i));
    CTVector pos_exp = 0.5 * Eigen::exp(I * matstack.h.row(i + 1) * (matstack.z0.cast<CMPLX>())(i));

    CMPLX k_frac = matstack.k(i) / matstack.k(i + 1); 
    CTVector f_fact = h_frac/k_frac;

    c.row(i + 1) = pos_exp * (c.row(i) * c_neg_exp * (1 + h_frac) + (boolValue(i) + cd.row(i)) * 
                   c_pos_exp * (1 - h_frac));

    cd.row(i + 1) = neg_exp * (c.row(i) * c_neg_exp * (1 - h_frac) + (boolValue(i) + cd.row(i)) * 
                    c_pos_exp * (1 + h_frac));

    f_perp.row(i + 1) = pos_exp * (f_perp.row(i) * c_neg_exp * (k_frac + f_fact) +
                        (boolValue(i) + fd_perp.row(i)) * c_pos_exp * (k_frac - f_fact));

    fd_perp.row(i + 1) = neg_exp * (f_perp.row(i) * c_neg_exp * (k_frac - f_fact) +
                         (boolValue(i) + fd_perp.row(i)) * c_pos_exp * (k_frac + f_fact));

    f_para.row(i + 1) = pos_exp * (f_para.row(i) * c_neg_exp * (k_frac + f_fact) +
                        (fd_para.row(i) - boolValue(i)) * c_pos_exp * (k_frac - f_fact));

    fd_para.row(i + 1) = neg_exp * (f_para.row(i) * c_neg_exp * (k_frac - f_fact) +
                        (fd_para.row(i) - boolValue(i)) * c_pos_exp * (k_frac + f_fact));
  }
  coeffs._c = std::move(c);
  coeffs._cd = std::move(cd);
  coeffs._f_perp = std::move(f_perp);
  coeffs._fd_perp = std::move(fd_perp);
  coeffs._f_para = std::move(f_para);
  coeffs._fd_para = std::move(fd_para);
}

void BaseSolver::calculateLifetime(Vector& bPerp, Vector& bPara)
{

  CVector bTmp(matstack.u.size());
  CVector bTmp2(matstack.u.size());

  bTmp = matstack.dX * (3.0 / 2.0) * Eigen::pow(Eigen::cos(matstack.x), 3);
  bTmp *= (coeffs._f_perp(dipoleLayer, Eigen::seqN(0, matstack.u.size())) +
           coeffs._fd_perp(dipoleLayer, Eigen::seqN(0, matstack.u.size())));
  bPerp = bTmp.real();

  bTmp2 = (coeffs._c(dipoleLayer, Eigen::seqN(0, matstack.u.size())) +
           coeffs._cd(dipoleLayer, Eigen::seqN(0, matstack.u.size())));
  bTmp = Eigen::pow(Eigen::sin(matstack.x.head(matstack.x.size())), 2);
  bTmp *= (coeffs._f_para(dipoleLayer, Eigen::seqN(0, matstack.u.size())) -
           coeffs._fd_para(dipoleLayer, Eigen::seqN(0, matstack.u.size())));
  bTmp += bTmp2;
  bTmp *= matstack.dX * (3.0 / 4.0) * Eigen::cos(matstack.x.head(matstack.x.size()));
  bPara = bTmp.real();
}

void BaseSolver::calculateDissPower(const double bPerpSum, const double bParaSum)
{
  // Power calculation
  CMatrix& pPerpP = resMap.get<CMatrix>("pPerpP");
  CMatrix& pParaP = resMap.get<CMatrix>("pParaP");
  CMatrix& pParaS = resMap.get<CMatrix>("pParaS");

  pPerpP.resize(matstack.numLayers - 1, matstack.u.size());
  pPerpP.resize(matstack.numLayers - 1, matstack.u.size());
  pParaS.resize(matstack.numLayers - 1, matstack.u.size());

  double q = 1.0; // PLQY
  CMPLX I(0.0, 1.0);

  Vector boolValue = Vector::Zero(matstack.numLayers);
  boolValue(dipoleLayer) = 1.0;
  for (Eigen::Index i = 0; i < matstack.numLayers - 1; ++i) {

    CTVector neg_exp = Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i));
    CTVector pos_exp = Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i));
    CVector ep_coeff = Eigen::sqrt(matstack.epsilon(i) / matstack.epsilon(dipoleLayer) - Eigen::pow(matstack.u, 2));

    pPerpP.row(i) = (-3.0 * q / 4.0) * ((Eigen::pow(matstack.u, 3)) / Eigen::abs(1 - Eigen::pow(matstack.u, 2))) * 
                            (ep_coeff) * (std::conj(std::sqrt(matstack.epsilon(i))) / std::sqrt(matstack.epsilon(i)));

    pPerpP.row(i) *= (coeffs._f_perp(i, Eigen::seqN(0, pPerpP.cols())) * neg_exp) -
                             ((coeffs._fd_perp(i, Eigen::seqN(0, pPerpP.cols())) + boolValue(i)) * pos_exp);

    pPerpP.row(i) *= (Eigen::conj((coeffs._f_perp(i, Eigen::seqN(0, pPerpP.cols())) * neg_exp) +
                             ((coeffs._fd_perp(i, Eigen::seqN(0, pPerpP.cols())) + boolValue(i)) * pos_exp)));


    pParaS.row(i) = (-3.0 * q / 8.0) * (matstack.u * Eigen::conj(ep_coeff)) / (Eigen::abs(1 - Eigen::pow(matstack.u, 2)));

    pParaS.row(i) *= (coeffs._c(i, Eigen::seqN(0, pParaS.cols())) * neg_exp) + 
                             ((coeffs._cd(i, Eigen::seqN(0, pParaS.cols())) + boolValue(i)) * pos_exp);

    pParaS.row(i) *= (Eigen::conj((coeffs._c(i, Eigen::seqN(0, pParaS.cols())) * neg_exp) -
                             ((coeffs._cd(i, Eigen::seqN(0, pParaS.cols())) + boolValue(i)) * pos_exp)));


    pParaP.row(i) = (-3.0 * q / 8.0) * (matstack.u * ep_coeff) * 
                            (std::conj(std::sqrt(matstack.epsilon(i))) / std::sqrt(matstack.epsilon(i)));

    pParaP.row(i) *= (coeffs._f_para(i, Eigen::seqN(0, pParaP.cols())) * neg_exp) -
                             ((coeffs._fd_para(i, Eigen::seqN(0, pParaP.cols())) - boolValue(i)) * pos_exp);

    pParaP.row(i) *= (Eigen::conj((coeffs._f_para(i, Eigen::seqN(0, pParaP.cols())) * neg_exp) +
                             ((coeffs._fd_para(i, Eigen::seqN(0, pParaP.cols())) - boolValue(i)) * pos_exp)));
  }

  // Fraction power calculation
  Matrix m1 = Eigen::real(pPerpP.block(0, 0, pPerpP.rows() - 1, pPerpP.cols()));
  Matrix m2 = Eigen::real(pPerpP.block(1, 0, pPerpP.rows() - 1, pPerpP.cols()));
  resMap.get<Matrix>("fpPerpP") = Eigen::abs(m2 - m1);
  resMap.get<Matrix>("fpPerpP") /= std::abs(bPerpSum);

  Matrix m3 = Eigen::real(pParaP.block(0, 0, pParaP.rows() - 1, pParaP.cols()));
  Matrix m4 = Eigen::real(pParaP.block(1, 0, pParaP.rows() - 1, pParaP.cols()));
  resMap.get<Matrix>("fpParaP") = Eigen::abs(m4 - m3);
  resMap.get<Matrix>("fpParaP") /= std::abs(bParaSum);

  Matrix m5 = Eigen::real(pParaS.block(0, 0, pParaS.rows() - 1, pParaS.cols()));
  Matrix m6 = Eigen::real(pParaS.block(1, 0, pParaS.rows() - 1, pParaS.cols()));
  resMap.get<Matrix>("fpParaS") = Eigen::abs(m6 - m5);
  resMap.get<Matrix>("fpParaS") /= std::abs(bParaSum);
}

void BaseSolver::calculate()
{

  // Loggin
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Starting calculation             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";

  double q = 1.0; // PLQY. will become a member in the future

  // Dipole lifetime calculations
  Vector bPerp(matstack.numKVectors - 1);
  Vector bPara(matstack.numKVectors - 1);

  calculateFresnelCoeffs();
  calculateGFCoeffRatios();
  calculateGFCoeffs();

  calculateLifetime(bPerp, bPara);
  double bPerpSum = 1.0 - q + q * (1.0 + bPerp.sum());
  double bParaSum = 1.0 - q + q * (1.0 + bPara.sum());
  calculateDissPower(bPerpSum, bParaSum);

  // normalizing dissipated power by alpha to get efficiency
  resMap("fpPerpP") *= alpha;
  resMap("fpParaP", "fpParaS") *= (1 - alpha);

  // Loggin
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Calculation finished!             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";
}

void BaseSolver::calculateWithSpectrum()
{
  PolyMap<ResTypes> tmp;
  tmp["tpPerpP"] = CMatrix::Zero(matstack.numLayers - 1, matstack.u.size());
  tmp["tpParaP"] = CMatrix::Zero(matstack.numLayers - 1, matstack.u.size());
  tmp["tpParaS"] = CMatrix::Zero(matstack.numLayers - 1, matstack.u.size());

  double dX = _spectrum(1, 0) - _spectrum(0, 0); // CHANGE IF DISCRETIZATION BECOMES UNEQUALLY SPACED
  for (Eigen::Index i = 0; i < _spectrum.rows(); ++i) {
    wvl = _spectrum(i, 0);
    this->discretize();
    calculate();
    // Integration
    if (i == 0 || i == _spectrum.rows() - 1) {
      resMap("pPerpP", "pParaP", "pParaS") *= 0.5;
    }
    tmp("tpPerpP", "tpParaP", "tpParaS") += tmp("tpPerpP", "tpParaP", "tpParaS") * std::complex<double>(0.5,0);//_spectrum(i, 1);
  }
  resMap("pPerpP", "pParaP", "pParaS") = tmp("tpPerpP", "tpParaP", "tpParaS") * dX;
}

void BaseSolver::calculateWithDipoleDistribution()
{
  PolyMap<ResTypes> tmp;
  tmp["tpPerpP"] = tmp["tpParaP"] = tmp["tpParaS"] = CMatrix::Zero(matstack.numLayers - 1, matstack.u.size());

  double dX = _dipolePositions(1) - _dipolePositions(0);
  double thickness = layers[dipoleLayer].getThickness();
  for (Eigen::Index i = 0; i < _dipolePositions.size(); ++i) {
    dipolePosition = _dipolePositions(i);
    this->discretize();
    calculateWithSpectrum();
    // Integration
    if (i == 0 || i == _dipolePositions.size() - 1) {
      resMap("pPerpP", "pParaP", "pParaS") *= 0.5;
    }
    tmp("tpPerpP", "tpParaP", "tpParaS") += resMap("pPerpP", "pParaP", "pParaS");
  }
  resMap("pPerpP", "pParaP", "pParaS") = tmp("tpPerpP", "tpParaP", "tpParaS") * dX / thickness;
}

void BaseSolver::run()
{
  if ((_spectrum.rows() == 1) && _dipolePositions.isZero()) { calculate(); }
  else if (_dipolePositions.isZero()) {
    calculateWithSpectrum();
  }
  else {
    calculateWithDipoleDistribution();
  }
}

void BaseSolver::calculateEmissionSubstrate()
{
  Vector thetaGlass;
  CMatrix& pPerpP = resMap.get<CMatrix>("pParaP");
  CMatrix& pParaP = resMap.get<CMatrix>("pParaP");
  CMatrix& pParaS = resMap.get<CMatrix>("pParaS");

  double uCriticalGlass =
    std::real(std::sqrt(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer)));
  auto uGlassIt =
    std::find_if(matstack.u.begin(), matstack.u.end(), [uCriticalGlass](auto a) { return a > uCriticalGlass; });
  auto uGlassIndex = uGlassIt - matstack.u.begin();

  thetaGlass = Eigen::real(Eigen::acos(Eigen::sqrt(
    1 - matstack.epsilon(dipoleLayer) / matstack.epsilon(matstack.numLayers - 1) * Eigen::pow(matstack.u, 2))));

  resMap.get<Vector>("pPerpPSub") = ((Eigen::real(pPerpP.row(matstack.numLayers - 2))) *
                    std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer))));
  resMap("pPerpPSub") /= Eigen::tan(thetaGlass);

  //CMatrix powerParaUTot = pParaP + pParaS;

  resMap.get<Vector>("pParaPSub") = ((Eigen::real(pParaP.row(matstack.numLayers - 2))) *
                        std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer))));
  resMap("pParaPSub") /= Eigen::tan(thetaGlass);

  resMap.get<Vector>("pParaSSub") = ((Eigen::real(pParaS.row(matstack.numLayers - 2))) *
                        std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer))));
  resMap("pPerpSSub")/= Eigen::tan(thetaGlass);
}

Vector const& BaseSolver::getInPlaneWavevector() const { return matstack.u; }

Eigen::Index BaseSolver::getDipoleIndex() const { return dipoleLayer; }

Distribution::Distribution(double xLeft, double xRight, size_t numPoints) :
  lBound{xLeft},
  hBound{xRight}
{
  values = Eigen::ArrayXd::LinSpaced(numPoints, xLeft, xRight);
}

Distribution::Distribution(double value) :
  lBound(value),
  hBound(value)
{
  values = Vector::Constant(1, value);
}

NormalDistribution::NormalDistribution(double xmin, double xmax, double x0, double sigma, size_t numPoints) :
  Distribution(xmin, xmax, numPoints)
{
  values = (1.0 / sqrt(2 * M_PI * pow(sigma, 2))) * (-0.5 * ((values - x0) / sigma).pow(2)).exp();
}