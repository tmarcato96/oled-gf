#define _USE_MATH_DEFINES
#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>

#include "linalg.hpp"
#include <matlayer.hpp>
#include <simulation.hpp>
#include <utils.hpp>

void Simulation::genInPlaneWavevector()
{
  // Cumulative sum of thicknesses
  matstack.z0.resize(matstack.numLayers - 1);
  matstack.z0(0) = 0.0;
  std::vector<double> thicknesses;
  for (size_t i = 1; i < layers.size() - 1; ++i) { thicknesses.push_back(layers[i].getThickness()); }
  std::partial_sum(thicknesses.begin(), thicknesses.end(), std::next(matstack.z0.begin()), std::plus<double>());
  matstack.z0 -= (matstack.z0(dipoleLayer - 1) + dipolePosition);

  // Discretization of in-plane wavevector
  CMPLX I(0.0, 1.0);
  double x_res = 5e-4;
  CVector x_range;

  if (_mode == SimulationMode::AngleSweep) {
    x_range = arange<Vector>(_sweepStart * M_PI / 180, _sweepStop * M_PI / 180 + x_res, x_res);

    matstack.x = x_range.head(x_range.size() - 1);
    matstack.u = Eigen::real(Eigen::sqrt(
      matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer) * (1 - pow(Eigen::cos(matstack.x), 2))));
  }
  // x_init is real and x_end is complex
  else if (_mode == SimulationMode::ModeDissipation) {
    Vector x_real = arange<Vector>(-std::acos(_sweepStart), -x_res, x_res);
    CVector x_imag = I * arange<Vector>(x_res, -std::acos(std::complex<double>(_sweepStop, 0.0)).imag() + x_res, x_res);

    x_range.resize(x_real.rows() + x_imag.rows());
    x_range.head(x_real.size()) = x_real;
    x_range.tail(x_imag.size()) = -x_imag;
    matstack.x = x_range.head(x_range.size() - 1);
    matstack.u = matstack.x.cos().real();
  }
  else {
    throw std::runtime_error("Invalid mode!");
  }

  matstack.numKVectors = matstack.u.size();

  // Differences (last element of head handled by const initialization)
  matstack.dX = x_range.segment(1, x_range.size() - 1) - x_range.segment(0, x_range.size() - 1);
  // matstack.dU =
  //   x_range.segment(1, x_range.size() - 1).cos().real() - x_range.segment(0, x_range.size() - 1).cos().real();
  matstack.dU = matstack.u.segment(1, matstack.u.size() - 1) - matstack.u.segment(0, matstack.u.size() - 1);
}

void Simulation::genOutofPlaneWavevector()
{
  // Out of plane wavevector
  matstack.k = 2 * M_PI / wvl / 1e-9 * matstack.epsilon.sqrt();
  matstack.h.resize(matstack.numLayers, matstack.u.size());
  matstack.h = matstack.k(dipoleLayer) *
               (((matstack.epsilon.replicate(1, matstack.u.size())) / matstack.epsilon(dipoleLayer)).rowwise() -
                 matstack.u.pow(2).transpose())
                 .sqrt();
}

void Simulation::discretize()
{
  loadMaterialData();
  genInPlaneWavevector();
  genOutofPlaneWavevector();
}

void Simulation::update()
{
  // Log initialization of Simulation
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Initializing Simulation             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";
  this->discretize();
}

Simulation::Simulation(SimulationMode mode,
  const std::vector<Layer>& layers,
  const double sweepStart,
  const double sweepStop,
  const double alpha) :
  BaseSolver(layers, sweepStart, sweepStop, alpha),
  _mode{mode}
{}

void Simulation::calculateEmissionSubstrate()
{
  Vector thetaGlass;
  CMatrix& powerPerpUpPol = resultTree.get<CMatrix>("P_perp_u");
  CMatrix& powerParaUpPol = resultTree.get<CMatrix>("P_para_p_u");
  CMatrix& powerParaUsPol = resultTree.get<CMatrix>("P_para_s_u");

  resultTree.insertAs<Vector>(Vector(), POWER_DIPOLES_SUB);

  thetaGlass = Eigen::real(Eigen::acos(Eigen::sqrt(
    1 - matstack.epsilon(dipoleLayer) / matstack.epsilon(matstack.numLayers - 1) * Eigen::pow(matstack.u, 2))));
  resultTree["angle"] = thetaGlass;
  Vector apodization = Eigen::tan(thetaGlass);

  resultTree.get<Vector>("P_perp_sub") =
    ((Eigen::real(powerPerpUpPol.row(matstack.numLayers - 2))) *
      std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer))));

  resultTree.get<Vector>("P_para_p_sub") =
    ((Eigen::real(powerParaUpPol.row(matstack.numLayers - 2))) *
      std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer))));

  resultTree.get<Vector>("P_para_s_sub") =
    ((Eigen::real(powerParaUsPol.row(matstack.numLayers - 2))) *
      std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer))));

  resultTree(POWER_DIPOLES_SUB) /= apodization;
}

void Simulation::calculateOutcoupling()
{
  Eigen::Index N = matstack.numLayers - 2;
  double uCrit = std::sqrt(1 / std::real(matstack.epsilon(dipoleLayer)));
  auto uRadPtr = std::find_if(matstack.u.begin(), matstack.u.end(), [uCrit](double i) { return i > uCrit; });

  double uCritGlass = std::sqrt(std::real(matstack.epsilon(N + 1)) / std::real(matstack.epsilon(dipoleLayer)));
  auto uGlassPtr =
    std::find_if(matstack.u.begin(), matstack.u.end(), [uCritGlass](double i) { return i > uCritGlass; });
  Eigen::Index uGlassIndex = uGlassPtr - matstack.u.begin();

  auto uWgPtr = std::find_if(matstack.u.begin(), matstack.u.end(), [](double i) { return i > 1.0; });
  Eigen::Index uWgIndex = uWgPtr - matstack.u.begin();

  Eigen::Index uRadIndex = uRadPtr - matstack.u.begin();
  CMatrix& powerPerpUpPol = resultTree.get<CMatrix>("P_perp_u");
  CMatrix& powerParaUpPol = resultTree.get<CMatrix>("P_para_p_u");
  CMatrix& powerParaUsPol = resultTree.get<CMatrix>("P_para_s_u");

  Matrix& fracPerp = resultTree.get<Matrix>("P_perp_uf");
  Matrix& fracParaP = resultTree.get<Matrix>("P_para_p_uf");
  Matrix& fracParaS = resultTree.get<Matrix>("P_para_s_uf");

  auto slice = Eigen::seq(0, uRadIndex - 1);
  auto sliceGlass = Eigen::seq(0, uGlassIndex - 1);
  auto sliceWg = Eigen::seq(uGlassIndex - 1, uWgIndex - 1);

  // Outcoupling
  const auto outcoupling_perp = (matstack.dU(slice) * powerPerpUpPol(N, slice).real().transpose()).sum() / bPerpSum;
  const auto outcoupling_para_p = (matstack.dU(slice) * powerParaUpPol(N, slice).real().transpose()).sum() / bParaSum;
  const auto outcoupling_para_s = (matstack.dU(slice) * powerParaUsPol(N, slice).real().transpose()).sum() / bParaSum;
  const auto outcoupling_para = outcoupling_para_p + outcoupling_para_s;
  resultTree["outcoupling_perp"] = outcoupling_perp;
  resultTree["outcoupling_para"] = outcoupling_para;

  // Substrate modes
  const auto glass_perp =
    (matstack.dU(sliceGlass) * powerPerpUpPol(N, sliceGlass).real().transpose()).sum() / bPerpSum - outcoupling_perp;
  const auto glass_para_p =
    (matstack.dU(sliceGlass) * powerParaUpPol(N, sliceGlass).real().transpose()).sum() / bParaSum - outcoupling_para_p;
  const auto glass_para_s =
    (matstack.dU(sliceGlass) * powerParaUsPol(N, sliceGlass).real().transpose()).sum() / bParaSum - outcoupling_para_s;
  const auto glass_para = glass_para_p + glass_para_s;
  resultTree["substrate_perp"] = glass_perp;
  resultTree["substrate_para"] = glass_para;

  double wg_perp = 0.0, wg_para = 0.0;
  for (Eigen::Index i = 1; i < N; ++i) {
    const double epsiR = layers[toSize(i)].getMaterial().getEpsilon(wvl).real();
    if (epsiR < 0.0) continue;
    double tmpFPerp = (matstack.dU(sliceWg) * fracPerp(i, sliceWg).transpose()).sum();
    double tmpFP = (matstack.dU(sliceWg) * fracParaP(i, sliceWg).transpose()).sum();
    double tmpFS = (matstack.dU(sliceWg) * fracParaS(i, sliceWg).transpose()).sum();
    double tmp = tmpFP + tmpFS;
    wg_perp += tmpFPerp;
    wg_para += tmp;
  }
  resultTree["waveguide_perp"] = wg_perp;
  resultTree["waveguide_para"] = wg_para;

  resultTree["evanescent_perp"] = 1 - outcoupling_perp - glass_perp - wg_perp;
  resultTree["evanescent_para"] = 1 - outcoupling_para - glass_para - wg_para;
}