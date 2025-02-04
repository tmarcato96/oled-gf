#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <material.hpp>
#include <data.hpp>
#include <fitting.hpp>
#include <simulation.hpp>

#include <matplot/matplot.h>

int main() {
  // Set up stack
  double wavelength = 456;
  std::vector<Layer> layers;

  layers.emplace_back(Material(1.0, 0.0), -1.0);
  layers.emplace_back(Material(1.7, 0.0), 35e-9, true);
  layers.emplace_back(Material(1.52, 0.0), 5000e-9);
  layers.emplace_back(Material(1.52, 0.0), -1.0);

  // Fitting filepath
  //const std::string targetToFit("/src/examples/data/setfos_simple_spectrum_isotropic.txt");
  const std::string targetToFit("/src/examples/data/3ML_processed.txt");
  // Spectrum
  double fwhm = 20;
  GaussianSpectrum spectrum(400, 700, wavelength, fwhm/2.355);
  // Dipole distribution
  DipoleDistribution dipoleDist(0.0, 35e-9, DipoleDistributionType::Uniform);
  // Create Solver
  auto solver = std::make_unique<Fitting>(targetToFit, layers, 17.5e-9, spectrum, 0.0, 90.0);
  // Fit
  auto fitRes = solver->fitEmissionSubstrate();

}