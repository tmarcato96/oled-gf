#include <iostream>

#include <material.hpp>
#include <basesolver.hpp>
#include <simulation.hpp>

#include <Eigen/Core>
#include <matplot/matplot.h>

int main()
{
  // Set up stack
  std::vector<Layer> layers;

  layers.emplace_back(Material("/src/mat/air.csv", ','), -1.0);
  layers.emplace_back(Material("/src/mat/ag.csv", ','), 1000e-10);
  layers.emplace_back(Material("/src/mat/mg_palik.csv", ','), 500e-10);
  layers.emplace_back(Material("/src//mat/tpd.csv", ','), 200e-10);
  layers.emplace_back(Material("/src/mat/alq3_literature.csv", ','), 500e-10, true);
  layers.emplace_back(Material("/src/mat/tpd.csv", ','), 200e-10);
  layers.emplace_back(Material("/src/mat/pedot.csv", ','), 300e-10);
  layers.emplace_back(Material("/src/mat/test_ito.csv", ','), 1600e-10);
  layers.emplace_back(Material("/src/mat/glass_no_loss.csv", ','), 5000e-10);
  layers.emplace_back(Material("/src/mat/glass_no_loss.csv", ','), -1.0);

  // Spectrum
  //THINGS TO DO: MOVE SIMULATION CONSTRUCTORS, CREATE NEW UNIFORM SPECTRUM MODE
  const double wavelength = 535;
  //DipoleDistribution dipolePositions{};
  //dipolePositions.dipolePositions = Vector::LinSpaced(11, 0, 20e-9);

  // Create Solver
  auto simulation = std::make_unique<Simulation>(SimulationMode::ModeDissipation, layers, 0.0, wavelength, 0.0, std::cos(std::complex<double>(0.0, 1.8)).real());
  simulation->run();

  // Mode dissipation figure
  Vector const& u = simulation->getInPlaneWavevector();
  Vector const& y = simulation->mFracPowerPerpUpPol.row(1).head(u.size());
  Vector const& yParapPol = simulation->mFracPowerParaUpPol.row(1).head(u.size());
  Vector const& yParasPol = simulation->mFracPowerParaUsPol.row(1).head(u.size());

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