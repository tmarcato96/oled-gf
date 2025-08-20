#define _USE_MATH_DEFINES

#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <complex>
#include <fstream>
#include <iostream>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "basesolver.hpp"
#include "linalg.hpp"
#include <forwardDecl.hpp>
#include <utils.hpp>

BaseSolver::BaseSolver(const std::vector<Layer>& layers,
  const double sweepStart,
  const double sweepStop,
  const double inalpha) :
  layers{std::move(layers)},
  _sweepStart{sweepStart},
  _sweepStop{sweepStop},
  alpha{inalpha}
{
  dipoleLayer = 0;
  for (auto layer : layers) {
    if (layer.isEmitter) { break; }
    dipoleLayer++;
  }
  // Initialize result Tree
  resultTree.insertAs<CMatrix>(CMatrix(), POWER_DIPOLES_U);
  resultTree.insertAs<Matrix>(Matrix(), POWER_DIPOLES_U_FRAC);
}

void BaseSolver::loadMaterialData()
{
  // Logging
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "                      Loading material data                      \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";

  matstack.numLayers = toIndex(layers.size());
  matstack.numInterfaces = matstack.numLayers - 1;
  matstack.numLayersTop = dipoleLayer + 1;
  matstack.numLayersBottom = matstack.numLayers - dipoleLayer;

  matstack.epsilon.resize(matstack.numLayers);
  for (Eigen::Index i = 0; i < matstack.numLayers; ++i) {
    matstack.epsilon(i) = layers[toSize(i)].getMaterial().getEpsilon(wvl);
    std::cout << "Layer " << i << "; Material: (" << matstack.epsilon(i).real() << ", " << matstack.epsilon(i).imag()
              << ")\n";
  }
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
    CTVector neg_exp =
      Eigen::exp(-2.0 * I * matstack.h.row(indexFromTop + 1) * (matstack.z0.cast<CMPLX>())(indexFromTop));
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
    CTVector f_fact = h_frac / k_frac;

    c.row(i - 1) =
      pos_exp * ((boolValue(i) + c.row(i)) * c_neg_exp * (1 + h_frac) + cd.row(i) * c_pos_exp * (1 - h_frac));

    cd.row(i - 1) =
      neg_exp * ((boolValue(i) + c.row(i)) * c_neg_exp * (1 - h_frac) + cd.row(i) * c_pos_exp * (1 + h_frac));

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

    // THE FOLLOWING DEFINITIONS ARE SLIGHTLY DIFFERENT FROM BEFORE DUE TO THE LOOP'S DIRECTION
    CTVector h_frac = matstack.h.row(i) / matstack.h.row(i + 1);

    CTVector c_neg_exp = Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i));
    CTVector c_pos_exp = Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i));

    CTVector neg_exp = 0.5 * Eigen::exp(-I * matstack.h.row(i + 1) * (matstack.z0.cast<CMPLX>())(i));
    CTVector pos_exp = 0.5 * Eigen::exp(I * matstack.h.row(i + 1) * (matstack.z0.cast<CMPLX>())(i));

    CMPLX k_frac = matstack.k(i) / matstack.k(i + 1);
    CTVector f_fact = h_frac / k_frac;

    c.row(i + 1) =
      pos_exp * (c.row(i) * c_neg_exp * (1 + h_frac) + (boolValue(i) + cd.row(i)) * c_pos_exp * (1 - h_frac));

    cd.row(i + 1) =
      neg_exp * (c.row(i) * c_neg_exp * (1 - h_frac) + (boolValue(i) + cd.row(i)) * c_pos_exp * (1 + h_frac));

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
  CMatrix& powerPerpUpPol = resultTree.get<CMatrix>("P_perp_u");
  CMatrix& powerParaUpPol = resultTree.get<CMatrix>("P_para_p_u");
  CMatrix& powerParaUsPol = resultTree.get<CMatrix>("P_para_s_u");

  powerPerpUpPol.resize(matstack.numLayers - 1, matstack.u.size());
  powerParaUpPol.resize(matstack.numLayers - 1, matstack.u.size());
  powerParaUsPol.resize(matstack.numLayers - 1, matstack.u.size());

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

    pParaS.row(i) =
      (-3.0 * q / 8.0) * (matstack.u * Eigen::conj(ep_coeff)) / (Eigen::abs(1 - Eigen::pow(matstack.u, 2)));

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
  Matrix m1 = Eigen::real(powerPerpUpPol.block(0, 0, powerPerpUpPol.rows() - 1, powerPerpUpPol.cols()));
  Matrix m2 = Eigen::real(powerPerpUpPol.block(1, 0, powerPerpUpPol.rows() - 1, powerPerpUpPol.cols()));
  resultTree.get<Matrix>("P_perp_uf") = Eigen::abs(m2 - m1) / std::abs(bPerpSum);

  Matrix m3 = Eigen::real(powerParaUpPol.block(0, 0, powerParaUpPol.rows() - 1, powerParaUpPol.cols()));
  Matrix m4 = Eigen::real(powerParaUpPol.block(1, 0, powerParaUpPol.rows() - 1, powerParaUpPol.cols()));
  resultTree.get<Matrix>("P_para_p_uf") = Eigen::abs(m4 - m3) / std::abs(bParaSum);

  Matrix m5 = Eigen::real(powerParaUsPol.block(0, 0, powerParaUsPol.rows() - 1, powerParaUsPol.cols()));
  Matrix m6 = Eigen::real(powerParaUsPol.block(1, 0, powerParaUsPol.rows() - 1, powerParaUsPol.cols()));
  resultTree.get<Matrix>("P_para_s_uf") = Eigen::abs(m6 - m5) / std::abs(bParaSum);
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
  resultTree("P_perp_uf") *= alpha;
  resultTree("P_para_p_uf", "P_para_s_uf") *= (1 - alpha);
  resultTree["u"] = matstack.u;

  // Loggin
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Calculation finished!             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";
}

void BaseSolver::run() { calculate(); }

Vector const& BaseSolver::getInPlaneWavevector() const { return matstack.u; }

size_t BaseSolver::getDipoleIndex() const { return toSize(dipoleLayer); }

void BaseSolver::setDipolePosition(double pos) { dipolePosition = pos; }

void BaseSolver::setWavelength(double wavelength) { wvl = wavelength; }

double BaseSolver::getLayerThickness(size_t index) { return layers[index].getThickness(); }