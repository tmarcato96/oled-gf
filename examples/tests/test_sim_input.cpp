#include <iostream>
#include <fstream>

#include <basesolver.hpp>
#include <matlayer.hpp>
#include <simulation.hpp>
#include <outdata.hpp>
#include <indata.hpp>

#include <Eigen/Core>
#include <matplot/matplot.h>

int main()
{
  // Create Solver
  auto importer = Data::ImportManager("C:\\Users\\mnouman\\oled-gf\\examples\\data\\simulation.json").makeImporter();
  auto solverJob = importer->solverFromFile();
  auto dipoleIndex = solverJob->getDipoleIndex();

  // Mode dissipation figure
  Vector const& u = solverJob->getInPlaneWavevector();
  Vector const& y = solverJob->mFracPowerPerpUpPol.row(dipoleIndex - 1).head(u.size());
  Vector const& yParapPol = solverJob->mFracPowerParaUpPol.row(dipoleIndex - 1).head(u.size());
  Vector const& yParasPol = solverJob->mFracPowerParaUsPol.row(dipoleIndex - 1).head(u.size());

  Data::Exporter exporter(*solverJob);
  std::ofstream output("C:\\Users\\mnouman\\oled-gf\\mat\\segfault.json");
  exporter.print(output);

  //Plot
  matplot::semilogy(u, y)->line_width(2).color("red");
  matplot::hold(matplot::on);
  matplot::semilogy(u, yParapPol)->line_width(2).color("blue");
  matplot::semilogy(u, yParasPol)->line_width(2).color("green");
  matplot::xlim({0.0, 2.0});
  matplot::xlabel("Normalized Wavevector");
  matplot::ylabel("Dissipated Power");
  matplot::show();
}