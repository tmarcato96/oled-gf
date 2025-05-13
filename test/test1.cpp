#include <filesystem>
#include <fstream>
#include <iostream>

#include <basesolver.hpp>
#include <matlayer.hpp>
#include <outdata.hpp>
#include <simulation.hpp>

int main()
{
  // Set up stack
  std::vector<Layer> layers;

  layers.emplace_back(Material(std::filesystem::path("./mat/air.csv"), ','), -1.0);
  layers.emplace_back(Material(std::filesystem::path("./mat/ag.csv"), ','), 200e-10);
  layers.emplace_back(Material(std::filesystem::path("./mat/mg_palik.csv"), ','), 1000e-10);
  layers.emplace_back(Material(std::filesystem::path("./mat/tpd.csv"), ','), 500e-10);
  layers.emplace_back(Material(std::filesystem::path("./mat/alq3_literature2.csv"), ','), 200e-10, true);
  layers.emplace_back(Material(std::filesystem::path("./mat/tpd.csv"), ','), 500e-10);
  layers.emplace_back(Material(std::filesystem::path("./mat/pedot.csv"), ','), 300e-10);
  layers.emplace_back(Material(std::filesystem::path("./mat/test_ito.csv"), ','), 1600e-10);
  layers.emplace_back(Material(std::filesystem::path("./mat/glass_no_loss.csv"), ','), 5000e-10);
  layers.emplace_back(Material(std::filesystem::path("./mat/glass_no_loss.csv"), ','), -1.0);

  const double wavelength = 535;

  // Create solver
  auto simulation = std::make_unique<Simulation>(
    SimulationMode::ModeDissipation, layers, 0.0, wavelength, 0.0, std::cos(std::complex<double>(0.0, 1.8)).real());
  simulation->run();

  // Export results
  Data::Exporter exporter(*simulation);
  std::ofstream outFile(std::filesystem::path("./test/out.json"));

  if (outFile.is_open()) {
    exporter.print(outFile);
    outFile.close();
  }
  else {
    std::cout << "Unable to open file!\n";
  }
}