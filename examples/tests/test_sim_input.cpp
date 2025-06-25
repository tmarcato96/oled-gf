#include <fstream>
#include <iostream>

#include <basesolver.hpp>
#include <indata.hpp>
#include <matlayer.hpp>
#include <outdata.hpp>
#include <simulation.hpp>

#include <Eigen/Core>

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
}