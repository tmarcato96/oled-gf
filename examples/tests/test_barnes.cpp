#include <iostream>

#include <material.hpp>
#include <basesolver.hpp>
#include <simulation.hpp>

#include <Eigen/Core>
#include <matplot/matplot.h>

int main()
{
  // Set up stack
  const double wavelength = 535;
  std::vector<Layer> layers;

  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\Al_Cent.csv", ','), -1.0);
  layers.emplace_back(Material(1.9, 0.0), 50e-9);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\CBP.csv", ','), 20e-9, true);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\PEDOT_BaytronP_AL4083.csv", ','), 35e-9);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\ITO.csv", ','), 150e-9);
  layers.emplace_back(Material(1.52, 0.0), 5000e-9);
  layers.emplace_back(Material(1.52, 0.0), -1.0);

  // Spectrum
  const double fwhm = 30;
  GaussianSpectrum spectrum(450, 700, wavelength, fwhm/2.355);

  // Create Solver
  auto simulation = std::make_unique<Simulation>(SimulationMode::ModeDissipation, layers, 10e-9, wavelength, 0.0, 2.0);
  simulation->run();

  // Mode dissipation figure
  Vector const& u = simulation->getInPlaneWavevector();
  Vector const& y = simulation->mFracPowerPerpUpPol.row(1).head(u.size());
  Vector const& yParapPol = simulation->mFracPowerParaUpPol.row(1).head(u.size());
  Vector const& yParasPol = simulation->mFracPowerParaUsPol.row(1).head(u.size());

  std::cout << y.size() << ' ' << y.head(10) << ' ' << y.tail(1) << std::endl;
  std::cout << yParapPol.size() << ' ' << yParapPol.head(1) << ' ' << yParapPol.tail(1) << std::endl;
  std::cout << yParasPol.size() << ' ' << yParasPol.head(1) << ' ' << yParasPol.tail(1) << std::endl;

  // Plot
  matplot::semilogy(u, y)->line_width(2).color("red");
  matplot::hold(matplot::on);
  matplot::semilogy(u, yParapPol)->line_width(2).color("blue");
  matplot::semilogy(u, yParasPol)->line_width(2).color("green");
  matplot::xlim({0.0, 2.0});
  matplot::xlabel("Normalized Wavevector");
  matplot::ylabel("Dissipated Power");
  matplot::show();
}