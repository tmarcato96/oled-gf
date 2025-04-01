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
#include <data.hpp>
#include "linalg.hpp"
#include <forwardDecl.hpp>


void BaseSolver::loadMaterialData()
{
  // Loggin
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Loading material data             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";

  matstack.numLayers = static_cast<Eigen::Index>(mLayers.size());
  matstack.numInterfaces = matstack.numLayers - 1;
  matstack.numLayersTop = mDipoleLayer + 1;
  matstack.numLayersBottom = matstack.numLayers - mDipoleLayer;

  matstack.epsilon.resize(matstack.numLayers);
  for (size_t i = 0; i < static_cast<size_t>(matstack.numLayers); ++i) {
    matstack.epsilon(i) = mLayers[i].getMaterial().getEpsilon(mWvl);
    std::cout << "Layer " << i << "; Material: (" << matstack.epsilon(i).real() << ", " << matstack.epsilon(i).imag()
              << ")\n";
  }
}

BaseSolver::BaseSolver(SimulationMode mode, 
      const std::vector<Layer>& layers,
      const double dipolePosition,
      const double wavelength,
      const double sweepStart,
      const double sweepStop) :
  _mode{mode},
  mLayers{std::move(layers)},
  mDipolePosition{dipolePosition},
  mWvl{wavelength},
  _sweepStart{sweepStart},
  _sweepStop{sweepStop}
{
  mDipoleLayer = 0;
  for (auto layer: layers) {
    if (layer.isEmitter) {
      break;
    }
    mDipoleLayer++;
  }
  _spectrum = Matrix::Zero(50, 2);
  _dipolePositions = Vector::Zero(10);
};

BaseSolver::BaseSolver(SimulationMode mode,
      const std::vector<Layer>& layers,
      const double dipolePosition,
      const std::string& spectrumFile,
      const double sweepStart,
      const double sweepStop) :
      BaseSolver(mode,
                 layers,
                 dipolePosition,
                 0.0,
                 sweepStart,
                 sweepStop)
{
  _spectrum = Data::loadFromFile(spectrumFile, 2);
}

BaseSolver::BaseSolver(SimulationMode mode,
      const std::vector<Layer>& layers,
      const double dipolePosition,
      const GaussianSpectrum& spectrum,
      const double sweepStart,
      const double sweepStop) :
      BaseSolver(mode,
                 layers,
                 dipolePosition,
                 0.0,
                 sweepStart,
                 sweepStop)
{
  _spectrum = std::move(spectrum.spectrum);
  _dipolePositions = Vector::Zero(10);
}

BaseSolver::BaseSolver(SimulationMode mode,
  const std::vector<Layer>& layers,
  const DipoleDistribution& dipoleDist,
  const double wavelength,
  const double sweepStart,
  const double sweepStop) :
  BaseSolver(mode,
             layers,
             0.0,
             wavelength,
             sweepStart,
             sweepStop)
{
_dipolePositions = std::move(dipoleDist.dipolePositions);
}

BaseSolver::BaseSolver(SimulationMode mode,
      const std::vector<Layer>& layers,
      const DipoleDistribution& dipoleDist,
      const GaussianSpectrum& spectrum,
      const double sweepStart,
      const double sweepStop) :
      BaseSolver(mode,
                 layers,
                 0.0,
                 spectrum,
                 sweepStart,
                 sweepStop)
{
  _dipolePositions = std::move(dipoleDist.dipolePositions);
}

void BaseSolver::calculateFresnelCoeffs()
{
  CMatrix R_perp(matstack.numInterfaces, matstack.numKVectors);
  CMatrix R_para(matstack.numInterfaces, matstack.numKVectors);

  R_perp = (matstack.h.block(1, 0, matstack.h.rows() - 1, matstack.h.cols()) -
             matstack.h.block(0, 0, matstack.h.rows() - 1, matstack.h.cols())) /
           (matstack.h.block(1, 0, matstack.h.rows() - 1, matstack.h.cols()) +
             matstack.h.block(0, 0, matstack.h.rows() - 1, matstack.h.cols()));
  (R_perp.bottomRows(R_perp.rows() - mDipoleLayer)) *= -1.0;

  R_para = ((matstack.h.block(0, 0, matstack.h.rows() - 1, matstack.h.cols())).colwise() *
              matstack.epsilon.segment(1, matstack.epsilon.size() - 1) -
            (matstack.h.block(1, 0, matstack.h.rows() - 1, matstack.h.cols())).colwise() *
              matstack.epsilon.segment(0, matstack.epsilon.size() - 1));
  R_para /= ((matstack.h.block(0, 0, matstack.h.rows() - 1, matstack.h.cols())).colwise() *
               matstack.epsilon.segment(1, matstack.epsilon.size() - 1) +
             (matstack.h.block(1, 0, matstack.h.rows() - 1, matstack.h.cols())).colwise() *
               matstack.epsilon.segment(0, matstack.epsilon.size() - 1));
  (R_para.bottomRows(R_para.rows() - mDipoleLayer)) *= -1.0;
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

  for (Eigen::Index i = 1; i < mDipoleLayer + 1; ++i) {
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

  for (Eigen::Index i = matstack.numLayers - mDipoleLayer - 2; i >= 0; --i) {
    Eigen::Index indexFromTop = i + mDipoleLayer;
    CT.row(i) =
      Eigen::exp(2.0 * I * matstack.h.row(indexFromTop) * (matstack.z0.cast<CMPLX>())(indexFromTop)) *
      (coeffs._Rperp.row(indexFromTop) + (CT.row(i + 1) * Eigen::exp(-2.0 * I * matstack.h.row(indexFromTop + 1) *
                                                                          (matstack.z0.cast<CMPLX>())(indexFromTop)))) /
      (1 + coeffs._Rperp.row(indexFromTop) *
             (CT.row(i + 1) *
               Eigen::exp(-2.0 * I * matstack.h.row(indexFromTop + 1) * (matstack.z0.cast<CMPLX>())(indexFromTop))));

    FT.row(i) =
      Eigen::exp(2.0 * I * matstack.h.row(indexFromTop) * (matstack.z0.cast<CMPLX>())(indexFromTop)) *
      (-coeffs._Rpara.row(indexFromTop) +
        (FT.row(i + 1) *
          Eigen::exp(-2.0 * I * matstack.h.row(indexFromTop + 1) * (matstack.z0.cast<CMPLX>())(indexFromTop)))) /
      (1 - coeffs._Rpara.row(indexFromTop) *
             (FT.row(i + 1) *
               Eigen::exp(-2.0 * I * matstack.h.row(indexFromTop + 1) * (matstack.z0.cast<CMPLX>())(indexFromTop))));
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

  c.row(mDipoleLayer) = (coeffs._cb.row(mDipoleLayer) + 1) * coeffs._ct.row(0) /
                        (1 - coeffs._cb.row(mDipoleLayer) * coeffs._ct.row(0));
  cd.row(mDipoleLayer) = (coeffs._ct.row(0) + 1) * coeffs._cb.row(mDipoleLayer) /
                         (1 - coeffs._cb.row(mDipoleLayer) * coeffs._ct.row(0));

  f_perp.row(mDipoleLayer) = (coeffs._fb.row(mDipoleLayer) + 1) * coeffs._ft.row(0) /
                             (1 - coeffs._fb.row(mDipoleLayer) * coeffs._ft.row(0));
  fd_perp.row(mDipoleLayer) = (coeffs._ft.row(0) + 1) * coeffs._fb.row(mDipoleLayer) /
                              (1 - coeffs._fb.row(mDipoleLayer) * coeffs._ft.row(0));

  f_para.row(mDipoleLayer) = (coeffs._fb.row(mDipoleLayer) - 1) * coeffs._ft.row(0) /
                             (1 - coeffs._fb.row(mDipoleLayer) * coeffs._ft.row(0));
  fd_para.row(mDipoleLayer) = (1 - coeffs._ft.row(0)) * coeffs._fb.row(mDipoleLayer) /
                              (1 - coeffs._fb.row(mDipoleLayer) * coeffs._ft.row(0));

  Vector boolValue = Vector::Zero(matstack.numLayers);
  boolValue(mDipoleLayer) = 1.0;

  for (Eigen::Index i = mDipoleLayer; i >= 1; --i) {
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

  for (Eigen::Index i = mDipoleLayer; i < matstack.numLayers - 1; ++i) {
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

  bTmp = matstack.dX * (3.0 / 2.0) * Eigen::pow(Eigen::cos(matstack.x.head(matstack.x.size())), 3);
  bTmp *= (coeffs._f_perp(mDipoleLayer, Eigen::seqN(0, matstack.u.size())) +
           coeffs._fd_perp(mDipoleLayer, Eigen::seqN(0, matstack.u.size())));
  bPerp = bTmp.real();

  bTmp2 = (coeffs._c(mDipoleLayer, Eigen::seqN(0, matstack.u.size())) +
           coeffs._cd(mDipoleLayer, Eigen::seqN(0, matstack.u.size())));
  bTmp = Eigen::pow(Eigen::sin(matstack.x.head(matstack.x.size())), 2);
  bTmp *= (coeffs._f_para(mDipoleLayer, Eigen::seqN(0, matstack.u.size())) -
           coeffs._fd_para(mDipoleLayer, Eigen::seqN(0, matstack.u.size())));
  bTmp += bTmp2;
  bTmp *= matstack.dX * (3.0 / 4.0) * Eigen::cos(matstack.x.head(matstack.x.size()));
  bPara = bTmp.real();
}

void BaseSolver::calculateDissPower(const double bPerpSum)
{
  // Power calculation
  mPowerPerpUpPol.resize(matstack.numLayers - 1, matstack.u.size());
  mPowerParaUpPol.resize(matstack.numLayers - 1, matstack.u.size());
  mPowerParaUsPol.resize(matstack.numLayers - 1, matstack.u.size());
  CMatrix powerParaTemp(matstack.numLayers - 1, matstack.u.size());

  double q = 1.0; // PLQY
  CMPLX I(0.0, 1.0);

  Vector boolValue = Vector::Zero(matstack.numLayers);
  boolValue(mDipoleLayer) = 1.0;
  for (Eigen::Index i = 0; i < matstack.numLayers - 1; ++i) {
    CMatrix temp = (-3.0 * q / 4.0) *
                         ((Eigen::pow(matstack.u, 3)) /
                           Eigen::abs(1 - Eigen::pow(matstack.u, 2))) *
                         (Eigen::sqrt(matstack.epsilon(i) / matstack.epsilon(mDipoleLayer) -
                                      Eigen::pow(matstack.u, 2))) *
                         (std::conj(std::sqrt(matstack.epsilon(i))) / std::sqrt(matstack.epsilon(i)));                  
    CMatrix temp1 = (coeffs._f_perp(i, Eigen::seqN(0, mPowerPerpUpPol.cols())) *
                            Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) -
                          ((coeffs._fd_perp(i, Eigen::seqN(0, mPowerPerpUpPol.cols())) + boolValue(i)) *
                            Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)));
    mPowerPerpUpPol.row(i) *= (Eigen::conj((coeffs._f_perp(i, Eigen::seqN(0, mPowerPerpUpPol.cols())) *
                                         Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) +
                                       ((coeffs._fd_perp(i, Eigen::seqN(0, mPowerPerpUpPol.cols())) + boolValue(i)) *
                                         Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)))));
    mPowerParaUsPol.row(i) = (-3.0 * q / 8.0) *
                         (matstack.u *
                           Eigen::conj(Eigen::sqrt(matstack.epsilon(i) / matstack.epsilon(mDipoleLayer) -
                                                   Eigen::pow(matstack.u, 2)))) /
                         (Eigen::abs(1 - Eigen::pow(matstack.u, 2)));
    mPowerParaUsPol.row(i) *= (coeffs._c(i, Eigen::seqN(0, mPowerParaUsPol.cols())) *
                            Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) +
                          ((coeffs._cd(i, Eigen::seqN(0, mPowerParaUsPol.cols())) + boolValue(i)) *
                            Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)));
    mPowerParaUsPol.row(i) *= (Eigen::conj((coeffs._c(i, Eigen::seqN(0, mPowerParaUsPol.cols())) *
                                         Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) -
                                       ((coeffs._cd(i, Eigen::seqN(0, mPowerParaUsPol.cols())) + boolValue(i)) *
                                         Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)))));

    mPowerParaUpPol.row(i) = (-3.0 * q / 8.0) *
                           (matstack.u *
                             Eigen::sqrt(matstack.epsilon(i) / matstack.epsilon(mDipoleLayer) -
                                         Eigen::pow(matstack.u, 2))) *
                           (std::conj(std::sqrt(matstack.epsilon(i))) / std::sqrt(matstack.epsilon(i)));
    mPowerParaUpPol.row(i) *= (coeffs._f_para(i, Eigen::seqN(0, mPowerParaUpPol.cols())) *
                              Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) -
                            ((coeffs._fd_para(i, Eigen::seqN(0, mPowerParaUpPol.cols())) - boolValue(i)) *
                              Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)));
    mPowerParaUpPol.row(i) *= (Eigen::conj((coeffs._f_para(i, Eigen::seqN(0, mPowerParaUpPol.cols())) *
                                           Eigen::exp(-I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i))) +
                                         ((coeffs._fd_para(i, Eigen::seqN(0, mPowerParaUpPol.cols())) - boolValue(i)) *
                                           Eigen::exp(I * matstack.h.row(i) * (matstack.z0.cast<CMPLX>())(i)))));
  }

  // Fraction power calculation
  Matrix m1 = Eigen::real(mPowerPerpUpPol.block(0, 0, mPowerPerpUpPol.rows() - 1, mPowerPerpUpPol.cols()));
  Matrix m2 = Eigen::real(mPowerPerpUpPol.block(1, 0, mPowerPerpUpPol.rows() - 1, mPowerPerpUpPol.cols()));
  mFracPowerPerpUpPol = Eigen::abs(m2 - m1);
  mFracPowerPerpUpPol /= std::abs(bPerpSum);

  Matrix m3 = Eigen::real(mPowerParaUpPol.block(0, 0, mPowerParaUpPol.rows() - 1, mPowerParaUpPol.cols()));
  Matrix m4 = Eigen::real(mPowerParaUpPol.block(1, 0, mPowerParaUpPol.rows() - 1, mPowerParaUpPol.cols()));
  mFracPowerParaUpPol = Eigen::abs(m4 - m3);
  mFracPowerParaUpPol /= std::abs(bPerpSum);

  Matrix m5 = Eigen::real(mPowerParaUsPol.block(0, 0, mPowerParaUsPol.rows() - 1, mPowerParaUsPol.cols()));
  Matrix m6 = Eigen::real(mPowerParaUsPol.block(1, 0, mPowerParaUsPol.rows() - 1, mPowerParaUsPol.cols()));
  mFracPowerParaUsPol = Eigen::abs(m6 - m5);
  mFracPowerParaUsPol /= std::abs(bPerpSum);
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
  double bPerpSum = 1.0 - q + q * (1 + bPerp.sum());
  double bParaSum = 1.0 - q + q * (1 + bPara.sum());

  calculateDissPower(bPerpSum);

  // Loggin
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Calculation finished!             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";
}

void BaseSolver::calculateWithSpectrum() {
  CMatrix pPerpUpPol = CMatrix::Zero(matstack.numLayers - 1, matstack.u.size());
  CMatrix pParaUpPol = CMatrix::Zero(matstack.numLayers - 1, matstack.u.size());
  CMatrix pParaUsPol = CMatrix::Zero(matstack.numLayers - 1, matstack.u.size());
  double dX = _spectrum(1, 0) - _spectrum(0, 0);
  for (Eigen::Index i=0; i < _spectrum.rows(); ++i) {
    mWvl = _spectrum(i, 0);
    this->discretize();
    calculate();
    // Integration
    if (i == 0 || i == _spectrum.rows() - 1) {
      mPowerPerpUpPol *= 0.5;
      mPowerParaUpPol *= 0.5;
      mPowerParaUsPol *= 0.5;
    }
    pPerpUpPol += (mPowerPerpUpPol * _spectrum(i, 1)) ;
    pParaUpPol += (mPowerParaUpPol * _spectrum(i, 1));
    pParaUsPol += (mPowerParaUsPol * _spectrum(i, 1));
  }
  mPowerPerpUpPol = pPerpUpPol * dX;
  mPowerParaUpPol = pParaUpPol * dX;
  mPowerParaUsPol = pParaUsPol * dX;
}

void BaseSolver::calculateWithDipoleDistribution() {
  CMatrix pPerpUpPol = CMatrix::Zero(matstack.numLayers - 1, matstack.u.size());
  CMatrix pParaUpPol = CMatrix::Zero(matstack.numLayers - 1, matstack.u.size());
  CMatrix pParaUsPol = CMatrix::Zero(matstack.numLayers - 1, matstack.u.size());
  double dX = _dipolePositions(1) - _dipolePositions(0);
  double thickness = mLayers[mDipoleLayer].getThickness();
  for (Eigen::Index i=0; i < _dipolePositions.size(); ++i) {
   mDipolePosition = _dipolePositions(i);
   this->discretize();
   calculateWithSpectrum();
    // Integration
    if (i == 0 || i == _dipolePositions.size() - 1) {
      mPowerPerpUpPol *= 0.5;
      mPowerParaUpPol *= 0.5;
      mPowerParaUsPol *= 0.5;
    }
    pPerpUpPol += (mPowerPerpUpPol) ;
    pParaUpPol += (mPowerParaUpPol);
    pParaUsPol += (mPowerParaUsPol);
  }
  mPowerPerpUpPol = pPerpUpPol * dX / thickness;
  mPowerParaUpPol = pParaUpPol * dX / thickness;
  mPowerParaUsPol = pParaUsPol * dX / thickness;
}

void BaseSolver::run() {
  if (_spectrum.isZero() && _dipolePositions.isZero()) {
    calculate();
  }
  else if (_dipolePositions.isZero()) {
    calculateWithSpectrum();
  }
  else {
    calculateWithDipoleDistribution();
  }
}

void BaseSolver::calculateEmissionSubstrate(Vector& thetaGlass, Vector& powerPerpGlass, Vector& powerParapPolGlass, Vector& powerParasPolGlass) const
{
  double uCriticalGlass = std::real(std::sqrt(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(mDipoleLayer)));
  auto uGlassIt =
    std::find_if(matstack.u.begin(), matstack.u.end(), [uCriticalGlass](auto a) { return a > uCriticalGlass; });
  auto uGlassIndex = uGlassIt - matstack.u.begin();

  thetaGlass =
    Eigen::real(Eigen::acos(Eigen::sqrt(1 - matstack.epsilon(mDipoleLayer) / matstack.epsilon(matstack.numLayers - 1) *
                                              Eigen::pow(matstack.u(Eigen::seq(1, uGlassIndex)), 2))));

  powerPerpGlass = ((Eigen::real(mPowerPerpUpPol(matstack.numLayers - 2, Eigen::seq(1, uGlassIndex)))) *
                    std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(mDipoleLayer))));
  powerPerpGlass /= Eigen::tan(thetaGlass);

  // CMatrix powerParaUTot = mPowerParaUpPol + mPowerParaUsPol;

  powerParapPolGlass = ((Eigen::real(mPowerParaUpPol(matstack.numLayers - 2, Eigen::seq(1, uGlassIndex)))) *
                    std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(mDipoleLayer))));
  powerParapPolGlass /= Eigen::tan(thetaGlass);

  powerParasPolGlass = ((Eigen::real(mPowerParaUsPol(matstack.numLayers - 2, Eigen::seq(1, uGlassIndex)))) *
                    std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(mDipoleLayer))));
  powerParasPolGlass /= Eigen::tan(thetaGlass);
}

Vector const& BaseSolver::getInPlaneWavevector() const {
  return matstack.u;
}

//void BaseSolver::writeToFile(Vector& powerPerpGlass){}

DipoleDistribution::DipoleDistribution(double zmin, double zmax, DipoleDistributionType type) {
  switch (type)
  {
  case DipoleDistributionType::Uniform:
    dipolePositions = Vector::LinSpaced(10, zmin, zmax);
    break;
  
  default:
    throw std::runtime_error("Unsupported dipole distribution");
  }
}

GaussianSpectrum::GaussianSpectrum(double xmin,
                                   double xmax,
                                   double x0,
                                   double sigma)
{
  spectrum.resize(50 , 2);
  Eigen::ArrayXd x = Eigen::ArrayXd::LinSpaced(50, xmin, xmax);
  spectrum.col(0) = x;
  spectrum.col(1) = (1.0/sqrt(2 * M_PI * pow(sigma, 2))) * (-0.5 * ((x - x0) / sigma).pow(2)).exp();
}