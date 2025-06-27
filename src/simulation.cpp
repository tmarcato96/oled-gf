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
  matstack.dU =
    x_range.segment(1, x_range.size() - 1).cos().real() - x_range.segment(0, x_range.size() - 1).cos().real();
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

void Simulation::init()
{
  // Log initialization of Simulation
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Initializing Simulation             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";
  this->discretize();
}

Simulation::SimRes Simulation::powerModeDissipation()
{
  const Eigen::Index N = matstack.u.rows();
  auto dipoleLayer = getDipoleIndex() - 1;

  std::vector<double> u(N), powerPerp(N), powerParaUs(N), powerParaUp(N);

  // Use Eigen::Map to copy Eigen arrays into std::vector
  Eigen::Map<Eigen::ArrayXd>(powerPerp.data(), N) = fracPowerPerpUpPol.row(dipoleLayer);
  Eigen::Map<Eigen::ArrayXd>(powerParaUs.data(), N) = fracPowerParaUsPol.row(dipoleLayer);
  Eigen::Map<Eigen::ArrayXd>(powerParaUp.data(), N) = fracPowerParaUpPol.row(dipoleLayer);
  Eigen::Map<Eigen::ArrayXd>(u.data(), N) = matstack.u;

  return Simulation::SimRes{u, powerPerp, powerParaUs, powerParaUp};
}

Simulation::Simulation(SimulationMode mode,
  const std::vector<Layer>& layers,
  const double dipolePosition,
  Spectrum<Distribution> spectrum,
  const double sweepStart,
  const double sweepStop,
  const double alpha) :
  BaseSolver(layers, dipolePosition, spectrum, sweepStart, sweepStop, alpha),
  _mode{mode}
{
  init();
}

Simulation::Simulation(SimulationMode mode,
  const std::vector<Layer>& layers,
  const Distribution& dipoleDist,
  Spectrum<Distribution> spectrum,
  const double sweepStart,
  const double sweepStop,
  const double alpha) :
  BaseSolver(layers, dipoleDist, spectrum, sweepStart, sweepStop, alpha),
  _mode{mode}
{
  init();
}