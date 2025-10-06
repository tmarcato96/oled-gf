#define _USE_MATH_DEFINES
#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
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
  resultTree["alpha"] = alpha;
}

void BaseSolver::loadMaterialData()
{
  // Loggin
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Loading material data             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";

  matstack.numLayers = toIndex(layers.size());
  matstack.numInterfaces = matstack.numLayers - 1;
  matstack.numLayersTop = dipoleLayer + 1;
  matstack.numLayersBottom = matstack.numLayers - dipoleLayer;

  matstack.epsilon.resize(matstack.numLayers);
  for (Eigen::Index i = 0; i < matstack.numLayers; ++i) {
    matstack.epsilon(i) = layers[toSize(i)].getMaterial().getEpsilon(wvl);
    std::cout << "Layer " << i << "; Material: (" << layers[toSize(i)].getMaterial().getEpsilon(wvl).real() << ", "
              << layers[toSize(i)].getMaterial().getEpsilon(wvl).imag() << ")\n";
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
  CMatrix CB = CMatrix::Zero(matstack.numLayersTop, matstack.numKVectors);
  CMatrix FB = CMatrix::Zero(matstack.numLayersTop, matstack.numKVectors);
  CMatrix CT = CMatrix::Zero(matstack.numLayersBottom, matstack.numKVectors);
  CMatrix FT = CMatrix::Zero(matstack.numLayersBottom, matstack.numKVectors);

  for (Eigen::Index i = 1; i < dipoleLayer + 1; ++i) {
    CB.row(i) =
      Eigen::exp(-2.0 * I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
      (coeffs._Rperp.row(i - 1) +
        (CB.row(i - 1) * Eigen::exp(2.0 * I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1)))) /
      (1 + coeffs._Rperp.row(i - 1) *
             (CB.row(i - 1) * Eigen::exp(2.0 * I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1))));

    FB.row(i) =
      Eigen::exp(-2.0 * I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
      (-coeffs._Rpara.row(i - 1) +
        (FB.row(i - 1) * Eigen::exp(2.0 * I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1)))) /
      (1 - coeffs._Rpara.row(i - 1) *
             (FB.row(i - 1) * Eigen::exp(2.0 * I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1))));
  }

  for (Eigen::Index i = matstack.numLayers - dipoleLayer - 2; i >= 0; --i) {
    Eigen::Index indexFromTop = i + dipoleLayer;
    CT.row(i) =
      Eigen::exp(2.0 * I * matstack.h.row(indexFromTop) * (matstack.z0.cast<CMPLX>())(indexFromTop)) *
      (coeffs._Rperp.row(indexFromTop) + (CT.row(i + 1) * Eigen::exp(-2.0 * I * matstack.h.row(indexFromTop + 1) *
                                                                     (matstack.z0.cast<CMPLX>())(indexFromTop)))) /
      (1 + coeffs._Rperp.row(indexFromTop) * (CT.row(i + 1) * Eigen::exp(-2.0 * I * matstack.h.row(indexFromTop + 1) *
                                                                         (matstack.z0.cast<CMPLX>())(indexFromTop))));

    FT.row(i) =
      Eigen::exp(2.0 * I * matstack.h.row(indexFromTop) * (matstack.z0.cast<CMPLX>())(indexFromTop)) *
      (-coeffs._Rpara.row(indexFromTop) + (FT.row(i + 1) * Eigen::exp(-2.0 * I * matstack.h.row(indexFromTop + 1) *
                                                                      (matstack.z0.cast<CMPLX>())(indexFromTop)))) /
      (1 - coeffs._Rpara.row(indexFromTop) * (FT.row(i + 1) * Eigen::exp(-2.0 * I * matstack.h.row(indexFromTop + 1) *
                                                                         (matstack.z0.cast<CMPLX>())(indexFromTop))));
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
  CMatrix c = CMatrix::Zero(matstack.numLayers, matstack.numKVectors);
  CMatrix cd = CMatrix::Zero(matstack.numLayers, matstack.numKVectors);
  CMatrix f_perp = CMatrix::Zero(matstack.numLayers, matstack.numKVectors);
  CMatrix fd_perp = CMatrix::Zero(matstack.numLayers, matstack.numKVectors);
  CMatrix f_para = CMatrix::Zero(matstack.numLayers, matstack.numKVectors);
  CMatrix fd_para = CMatrix::Zero(matstack.numLayers, matstack.numKVectors);

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
    c.row(i - 1) =
      0.5 * Eigen::exp(I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1)) *
      ((boolValue(i) + c.row(i)) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (1 + matstack.h.row(i) / matstack.h.row(i - 1)) +
        cd.row(i) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (1 - matstack.h.row(i) / matstack.h.row(i - 1)));

    cd.row(i - 1) =
      0.5 * Eigen::exp(-I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1)) *
      ((boolValue(i) + c.row(i)) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (1 - matstack.h.row(i) / matstack.h.row(i - 1)) +
        cd.row(i) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (1 + matstack.h.row(i) / matstack.h.row(i - 1)));

    f_perp.row(i - 1) =
      0.5 * Eigen::exp(I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1)) *
      ((boolValue(i) + f_perp.row(i)) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (matstack.k(i) / matstack.k(i - 1) +
            matstack.k(i - 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i - 1)) +
        fd_perp.row(i) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (matstack.k(i) / matstack.k(i - 1) -
            matstack.k(i - 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i - 1)));

    fd_perp.row(i - 1) =
      0.5 * Eigen::exp(-I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1)) *
      ((boolValue(i) + f_perp.row(i)) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (matstack.k(i) / matstack.k(i - 1) -
            matstack.k(i - 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i - 1)) +
        fd_perp.row(i) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (matstack.k(i) / matstack.k(i - 1) +
            matstack.k(i - 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i - 1)));

    f_para.row(i - 1) =
      0.5 * Eigen::exp(I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1)) *
      ((boolValue(i) + f_para.row(i)) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (matstack.k(i) / matstack.k(i - 1) +
            matstack.k(i - 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i - 1)) +
        fd_para.row(i) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (matstack.k(i) / matstack.k(i - 1) -
            matstack.k(i - 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i - 1)));

    fd_para.row(i - 1) =
      0.5 * Eigen::exp(-I * matstack.h.row(i - 1) * (matstack.z0.cast<CMPLX>())(i - 1)) *
      ((boolValue(i) + f_para.row(i)) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (matstack.k(i) / matstack.k(i - 1) -
            matstack.k(i - 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i - 1)) +
        fd_para.row(i) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i - 1)) *
          (matstack.k(i) / matstack.k(i - 1) +
            matstack.k(i - 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i - 1)));
  }

  for (Eigen::Index i = dipoleLayer; i < matstack.numLayers - 1; ++i) {
    c.row(i + 1) = 0.5 * Eigen::exp(I * matstack.h.row(i + 1) * (matstack.z0.cast<CMPLX>())(i)) *
                   (c.row(i) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
                       (1 + matstack.h.row(i) / matstack.h.row(i + 1)) +
                     (boolValue(i) + cd.row(i)) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
                       (1 - matstack.h.row(i) / matstack.h.row(i + 1)));

    cd.row(i + 1) = 0.5 * Eigen::exp(-I * matstack.h.row(i + 1) * (matstack.z0.cast<CMPLX>())(i)) *
                    (c.row(i) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
                        (1 - matstack.h.row(i) / matstack.h.row(i + 1)) +
                      (boolValue(i) + cd.row(i)) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
                        (1 + matstack.h.row(i) / matstack.h.row(i + 1)));

    f_perp.row(i + 1) =
      0.5 * Eigen::exp(I * matstack.h.row(i + 1) * (matstack.z0.cast<CMPLX>())(i)) *
      (f_perp.row(i) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
          (matstack.k(i) / matstack.k(i + 1) +
            matstack.k(i + 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i + 1)) +
        (boolValue(i) + fd_perp.row(i)) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
          (matstack.k(i) / matstack.k(i + 1) -
            matstack.k(i + 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i + 1)));

    fd_perp.row(i + 1) =
      0.5 * Eigen::exp(-I * matstack.h.row(i + 1) * (matstack.z0.cast<CMPLX>())(i)) *
      (f_perp.row(i) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
          (matstack.k(i) / matstack.k(i + 1) -
            matstack.k(i + 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i + 1)) +
        (boolValue(i) + fd_perp.row(i)) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
          (matstack.k(i) / matstack.k(i + 1) +
            matstack.k(i + 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i + 1)));

    f_para.row(i + 1) =
      0.5 * Eigen::exp(I * matstack.h.row(i + 1) * (matstack.z0.cast<CMPLX>())(i)) *
      (f_para.row(i) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
          (matstack.k(i) / matstack.k(i + 1) +
            matstack.k(i + 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i + 1)) +
        (fd_para.row(i) - boolValue(i)) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
          (matstack.k(i) / matstack.k(i + 1) -
            matstack.k(i + 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i + 1)));

    fd_para.row(i + 1) =
      0.5 * Eigen::exp(-I * matstack.h.row(i + 1) * (matstack.z0.cast<CMPLX>())(i)) *
      (f_para.row(i) * Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
          (matstack.k(i) / matstack.k(i + 1) -
            matstack.k(i + 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i + 1)) +
        (fd_para.row(i) - boolValue(i)) * Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)) *
          (matstack.k(i) / matstack.k(i + 1) +
            matstack.k(i + 1) / matstack.k(i) * matstack.h.row(i) / matstack.h.row(i + 1)));
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
    powerPerpUpPol.row(i) =
      (-3.0 * q / 4.0) * ((Eigen::pow(matstack.u, 3)) / Eigen::abs(1 - Eigen::pow(matstack.u, 2))) *
      (Eigen::sqrt(matstack.epsilon(i) / matstack.epsilon(dipoleLayer) - Eigen::pow(matstack.u, 2))) *
      (std::conj(std::sqrt(matstack.epsilon(i))) / std::sqrt(matstack.epsilon(i)));
    powerPerpUpPol.row(i) *= (coeffs._f_perp(i, Eigen::seqN(0, powerPerpUpPol.cols())) *
                               Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) -
                             ((coeffs._fd_perp(i, Eigen::seqN(0, powerPerpUpPol.cols())) + boolValue(i)) *
                               Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)));
    powerPerpUpPol.row(i) *= (Eigen::conj((coeffs._f_perp(i, Eigen::seqN(0, powerPerpUpPol.cols())) *
                                            Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) +
                                          ((coeffs._fd_perp(i, Eigen::seqN(0, powerPerpUpPol.cols())) + boolValue(i)) *
                                            Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)))));
    powerParaUsPol.row(i) =
      (-3.0 * q / 8.0) *
      (matstack.u *
        Eigen::conj(Eigen::sqrt(matstack.epsilon(i) / matstack.epsilon(dipoleLayer) - Eigen::pow(matstack.u, 2)))) /
      (Eigen::abs(1 - Eigen::pow(matstack.u, 2)));
    powerParaUsPol.row(i) *= (coeffs._c(i, Eigen::seqN(0, powerParaUsPol.cols())) *
                               Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) +
                             ((coeffs._cd(i, Eigen::seqN(0, powerParaUsPol.cols())) + boolValue(i)) *
                               Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)));
    powerParaUsPol.row(i) *= (Eigen::conj((coeffs._c(i, Eigen::seqN(0, powerParaUsPol.cols())) *
                                            Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) -
                                          ((coeffs._cd(i, Eigen::seqN(0, powerParaUsPol.cols())) + boolValue(i)) *
                                            Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)))));

    powerParaUpPol.row(i) =
      (-3.0 * q / 8.0) *
      (matstack.u * Eigen::sqrt(matstack.epsilon(i) / matstack.epsilon(dipoleLayer) - Eigen::pow(matstack.u, 2))) *
      (std::conj(std::sqrt(matstack.epsilon(i))) / std::sqrt(matstack.epsilon(i)));
    powerParaUpPol.row(i) *= (coeffs._f_para(i, Eigen::seqN(0, powerParaUpPol.cols())) *
                               Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) -
                             ((coeffs._fd_para(i, Eigen::seqN(0, powerParaUpPol.cols())) - boolValue(i)) *
                               Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)));
    powerParaUpPol.row(i) *= (Eigen::conj((coeffs._f_para(i, Eigen::seqN(0, powerParaUpPol.cols())) *
                                            Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) +
                                          ((coeffs._fd_para(i, Eigen::seqN(0, powerParaUpPol.cols())) - boolValue(i)) *
                                            Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)))));
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
  bPerpSum = 1.0 - q + q * (1.0 + bPerp.sum());
  bParaSum = 1.0 - q + q * (1.0 + bPara.sum());
  calculateDissPower(bPerpSum, bParaSum);

  // normalizing dissipated power by alpha to get efficiency
  // resultTree("P_perp_uf") *= alpha;
  // resultTree("P_para_p_uf", "P_para_s_uf") *= (1 - alpha);
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

double BaseSolver::getLayerThickness(size_t index) { return layers[index].getThickness(); }

double BaseSolver::getAlpha() const { return alpha; }

void BaseSolver::setDipolePosition(double pos) { dipolePosition = pos; }

void BaseSolver::setWavelength(double wavelength) { wvl = wavelength; }

void BaseSolver::setAlpha(double a) { alpha = a; }
