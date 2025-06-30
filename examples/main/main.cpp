#define _USE_MATH_DEFINES
#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <linalg.hpp>
#include <matlayer.hpp>
#include <memory>
#include <numeric>
#include <simulation.hpp>

#include <Eigen/Core>

int main()
{
  // Set up stack
  const double wavelength = 530;
  std::vector<Layer> layers;

  layers.emplace_back(Material(0.88, 6.4), -1.0);
  layers.emplace_back(Material(1.9, 0.0), 50e-9);
  layers.emplace_back(Material(1.78, 0.0), 20e-9, true);
  layers.emplace_back(Material(1.52, 0.0), 35e-9);
  layers.emplace_back(Material(1.91, 0.0), 150e-9);
  layers.emplace_back(Material(1.52, 0.0), 5000e-9);
  layers.emplace_back(Material(1.52, 0.0), -1.0);

  // Spectrum
  const double fwhm = 30;
  NormalDistribution dist{450, 700, wavelength, fwhm / 2.355, 50};
  Spectrum<Distribution> spectrum{dist};

  // Create Solver
  auto simulation = std::make_unique<Simulation>(SimulationMode::AngleSweep, layers, 10e-9, spectrum, 0.0, 90.0);

  simulation->run();
  // Polar figure
  Eigen::ArrayXd thetaGlass, powerPerpAngleGlass, powerParasPolAngleGlass, powerParapPolAngleGlass;
  simulation->calculateEmissionSubstrate(
    thetaGlass, powerPerpAngleGlass, powerParapPolAngleGlass, powerParasPolAngleGlass);

  // Save to file
  Vector thetaGlassDeg(thetaGlass.size());
  thetaGlassDeg = thetaGlass * 180 / M_PI;
  // saveToText("OLED_angle_dependent.csv", ',', {thetaGlassDeg, powerPerpAngleGlass, powerParapPolAngleGlass,
  // powerParasPolAngleGlass});
}