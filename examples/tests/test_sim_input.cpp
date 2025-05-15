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
  auto manager = Data::ImportManager("C:\\Users\\mnouman\\oled-gf\\examples\\data\\simulation.json");
  auto importer = manager.makeImporter();
  auto solverJob = importer->solverFromFile();
  solverJob->run();
  auto dipoleIndex = solverJob->getDipoleIndex();

  // Mode dissipation figure
  Vector const& u = solverJob->getInPlaneWavevector();
  Vector const& y = solverJob->fracPowerPerpUpPol.row(dipoleIndex - 1).head(u.size());
  Vector const& yParapPol = solverJob->fracPowerParaUpPol.row(dipoleIndex - 1).head(u.size());
  Vector const& yParasPol = solverJob->fracPowerParaUsPol.row(dipoleIndex - 1).head(u.size());


  std::ofstream output("C:\\Users\\mnouman\\oled-gf\\mat\\segfault.json");
  Data::Exporter exporter(*solverJob, output);
  exporter.print();

  //Plot
 // matplot::semilogy(u, y)->line_width(2).color("red");
 // matplot::hold(matplot::on);
 // matplot::semilogy(u, yParapPol)->line_width(2).color("blue");
 // matplot::semilogy(u, yParasPol)->line_width(2).color("green");
 // matplot::xlim({0.0, 2.0});
 // matplot::xlabel("Normalized Wavevector");
 // matplot::ylabel("Dissipated Power");
 // matplot::show();
}