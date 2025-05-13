#include <iostream>
#include <fstream>

#include <basesolver.hpp>
#include <matlayer.hpp>
#include <simulation.hpp>
#include <outdata.hpp>

#include <Eigen/Core>
#include <matplot/matplot.h>

int main()
{
  // Set up stack
  std::vector<Layer> layers;

  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\air.csv", ','), -1.0);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\ag.csv", ','), 200e-10);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\mg_palik.csv", ','), 1000e-10);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\tpd.csv", ','), 500e-10);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\alq3_literature2.csv", ','), 200e-10, true);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\tpd.csv", ','), 500e-10);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\pedot.csv", ','), 300e-10);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\test_ito.csv", ','), 1600e-10);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\glass_no_loss.csv", ','), 5000e-10);
  layers.emplace_back(Material("C:\\Users\\mnouman\\oled-gf\\mat\\glass_no_loss.csv", ','), -1.0);

  // Spectrum
  // THINGS TO DO: MOVE SIMULATION CONSTRUCTORS, CREATE NEW UNIFORM SPECTRUM MODE
  const double wavelength = 535;
  // DipoleDistribution dipolePositions{};
  // dipolePositions.dipolePositions = Vector::LinSpaced(11, 0, 20e-9);

  // Create Solver
  auto simulation = std::make_unique<Simulation>(
    SimulationMode::ModeDissipation, layers, 0.0, wavelength, 0.0, std::cos(std::complex<double>(0.0, 1.8)).real());
  simulation->run();
  auto dipoleIndex = simulation->getDipoleIndex();

  // Mode dissipation figure
  Vector const& u = simulation->getInPlaneWavevector();
  Vector const& y = simulation->mFracPowerPerpUpPol.row(dipoleIndex - 1).head(u.size());
  Vector const& yParapPol = simulation->mFracPowerParaUpPol.row(dipoleIndex - 1).head(u.size());
  Vector const& yParasPol = simulation->mFracPowerParaUsPol.row(dipoleIndex - 1).head(u.size());

  std::ofstream output("C:\\Users\\mnouman\\oled-gf\\mat\\segfault.json");
  Data::Exporter exporter(*simulation, output);
  exporter.print();

  // Plot
  //matplot::semilogy(u, y)->line_width(2).color("red");
  //matplot::hold(matplot::on);
  //matplot::semilogy(u, yParapPol)->line_width(2).color("blue");
  //matplot::semilogy(u, yParasPol)->line_width(2).color("green");
  //matplot::xlim({0.0, 2.0});
  //matplot::xlabel("Normalized Wavevector");
  //matplot::ylabel("Dissipated Power");
  //matplot::show();
}