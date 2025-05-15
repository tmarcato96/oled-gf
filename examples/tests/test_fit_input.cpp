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

  if(importer->getSolverMode() == Data::SolverMode::fitting) {
    Fitting fittingJob = dynamic_cast<Fitting&>(*solverJob);
    fittingJob.fitEmissionSubstrate();

    std::ofstream output("C:\\Users\\mnouman\\oled-gf\\mat\\segfault.json");
    Data::Exporter exporter(fittingJob, output);
    exporter.print();
  }
  else {throw std::runtime_error("Wrong job tipe!");}
}