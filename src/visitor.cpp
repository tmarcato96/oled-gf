#include <algorithm>
#include <array>
#include <complex>
#include <fstream>
#include <iostream>
#include <jsonsimplecpp/node.hpp>
#include <jsonsimplecpp/parser.hpp>
#include <map>
#include <optional>
#include <queue>

#include <distribution.hpp>
#include <fileutils.hpp>
#include <forwardDecl.hpp>
#include <matlayer.hpp>
#include <simulation.hpp>
#include <sweep.hpp>
#include <visitor.hpp>

void ConfigVisitor::operator()(const std::unique_ptr<JsonObject>& mapptr)
{
  _depth++;
  for (auto it = mapptr->begin(); it != mapptr->end(); it++) {
    if (it->second == nullptr)
      _depth > 1 ? _helperQueue.push(it->first) : throw std::runtime_error("File format error: empty field!");
    else {
      _helperQueue.push(it->first);
      it->second->traverse();
    }
    if (_depth == 1) fillBaseField();
  }
  _depth--;
}

void ConfigVisitor::operator()(const std::unique_ptr<JsonList>& listptr)
{
  _depth++;
  for (auto it = listptr->begin(); it != listptr->end(); it++) {
    if (*it == nullptr) {
      if (_depth > 1) continue;
      else throw std::runtime_error("File format error: empty field!");
    }
    else {
      (*it)->traverse();
    }
    if (_depth == 1) fillBaseField();
  }
  _depth--;
}

void ConfigVisitor::operator()(const std::string& val) { _helperQueue.push(val); }

void ConfigVisitor::operator()(const double val) { _helperQueue.push(val); }

void ConfigVisitor::fillBaseField()
{

  if (!std::holds_alternative<std::string>(_helperQueue.front()))
    throw std::runtime_error("misformatted JSON config file!");
  auto field = std::get<std::string>(_helperQueue.front());

  // fills layer
  if (field.substr(0, 5) == static_cast<std::string>("Layer") ||
      field.substr(0, 5) == static_cast<std::string>("layer")) {

    bool layerEmitter;
    double layerThickness;
    Material layerMat;
    size_t layerNum = std::stoi(field.substr(5, field.length() - 5));

    _helperQueue.pop();
    while (!_helperQueue.empty()) {
      auto subfield = std::get<std::string>(return_pop(_helperQueue));
      if (subfield == "thickness") layerThickness = std::get<double>(return_pop(_helperQueue));
      else if (subfield == "emitter") layerEmitter = static_cast<bool>(std::get<double>(return_pop(_helperQueue)));
      else if (subfield == "material") fillMaterialHelper(layerMat);
      else {
        throw std::runtime_error("wrongly formatted layer information!");
      }
    }

    Layer layer{layerMat, layerThickness, layerEmitter};
    _layerMap.insert(std::pair<int, Layer>{layerNum, layer});
  }

  // fills fitData
  else if (field == static_cast<std::string>("fitData") || field == static_cast<std::string>("FitData")) {

    _helperQueue.pop();
    // for CSV reference
    if (std::holds_alternative<std::string>(_helperQueue.front())) {
      auto subfield = std::get<std::string>(return_pop(_helperQueue));
      if (subfield.contains('/') || subfield.contains('\\')) { _fitFile = std::move(subfield); }
      else {
        throw std::runtime_error("misformatted path to intensities!");
      }
    }
    else {
      throw std::runtime_error("misformatted fitData filed in JSON config file!");
    }
  }

  // fills alpha
  else if (field == static_cast<std::string>("alpha") || field == static_cast<std::string>("Alpha")) {

    _helperQueue.pop();
    _alpha = std::get<double>(return_pop(_helperQueue));
  }

  // fills dipole
  else if (field == static_cast<std::string>("dipole") || field == static_cast<std::string>("Dipole")) {

    _helperQueue.pop();
    if (std::holds_alternative<std::string>(_helperQueue.front())) {
      auto subfield = std::get<std::string>(return_pop(_helperQueue));
      if (subfield == static_cast<std::string>("uniform")) fillDipoleModeHelper();
    }
    else {
      _dipoleDist = AnyDist(std::get<double>(return_pop(_helperQueue)));
    }
  }

  // fills spectrum
  else if (field == static_cast<std::string>("spectrum") || field == static_cast<std::string>("Spectrum")) {

    _helperQueue.pop();
    if (std::holds_alternative<std::string>(_helperQueue.front())) { fillSpectrumModeHelper(); }
    else {
      _spectrum = AnyDist(std::get<double>(return_pop(_helperQueue)));
    }
  }

  // fills simulation mode stuff
  else if (field == static_cast<std::string>("simtype") || field == static_cast<std::string>("Simtype")) {

    _helperQueue.pop();

    auto subfield = std::get<std::string>(return_pop(_helperQueue));
    if (subfield == "anglesweep" || subfield == "angleSweep") _simMode = SimulationMode::AngleSweep;
    if (subfield == "modedissipation" || subfield == "modeDissipation") _simMode = SimulationMode::ModeDissipation;
  }

  // fills sweep configuration
  else if (field == static_cast<std::string>("sweep") || field == static_cast<std::string>("Sweep")) {

    _helperQueue.pop();
    auto subfield = std::get<std::string>(return_pop(_helperQueue));
    if (subfield == "start") _sweepStart = std::get<double>(return_pop(_helperQueue));
    else {
      throw std::runtime_error("misformatted sweep settings");
    }
    subfield = std::get<std::string>(return_pop(_helperQueue));
    if (subfield == "stop") _sweepStop = std::get<double>(return_pop(_helperQueue));
    else {
      throw std::runtime_error("misformatted sweep settings");
    }
  }
  else {
    throw std::runtime_error("JSON config file contains ambiguous parameters!");
  }
}

void ConfigVisitor::fillMaterialHelper(Material& mat)
{

  // for CSV reference
  if (std::holds_alternative<std::string>(_helperQueue.front())) {
    auto subfield = std::get<std::string>(return_pop(_helperQueue));
    if (subfield.contains('/') || subfield.contains('\\')) {
      Material res(subfield, ',');
      mat = res;
    }
    else {
      throw std::runtime_error("misformatted material field for layer in JSON config file!");
    }
  }

  // for in-file intensities
  else if (std::holds_alternative<double>(_helperQueue.front())) {

    auto subfield = std::get<double>(return_pop(_helperQueue));
    double realRefIndex = subfield;

    subfield = std::get<double>(return_pop(_helperQueue));
    double imagRefIndex = subfield;

    Material res(realRefIndex, imagRefIndex);
    mat = res;
  }
}

void ConfigVisitor::fillDipoleModeHelper()
{
  // it doesn't matter if subfield is zmax or zmin for uniform, just fill two doubles and compare
  double zmin, zmax;
  size_t N;

  while (!_helperQueue.empty()) {
    auto subfield = std::get<std::string>(return_pop(_helperQueue));
    auto val = std::get<double>(return_pop(_helperQueue));
    if (subfield == "zmin") zmin = val;
    else if (subfield == "zmax") zmax = val;
    else if (subfield == "N") N = checked_narrow_cast<size_t>(val);
    else throw std::runtime_error("dipole distribution wrong formatting");
  }
  _dipoleDist = AnyDist(Distribution(zmin, zmax, N));
}

void ConfigVisitor::fillSpectrumModeHelper()
{
  auto distLabel = std::get<std::string>(return_pop(_helperQueue));

  if (distLabel == "gaussian") {
    double xmin;
    double xmax;
    double x0;
    double sigma;
    size_t N;

    while (!_helperQueue.empty()) {
      auto subfield = std::get<std::string>(return_pop(_helperQueue));
      auto val = std::get<double>(return_pop(_helperQueue));
      if (subfield == "xmin") xmin = val;
      else if (subfield == "xmax") xmax = val;
      else if (subfield == "x0") x0 = val;
      else if (subfield == "sigma") sigma = val;
      else if (subfield == "N") N = checked_narrow_cast<size_t>(val);
      else {
        throw std::runtime_error("misformatted spectrum info provided!");
      }
    }
    _spectrum = AnyDist(NormalDistribution(xmin, xmax, x0, sigma, N));
  }
  else if (distLabel == "file") {
    auto filePath = std::get<std::string>(return_pop(_helperQueue));
    if (filePath.contains('/') || filePath.contains('\\')) { _spectrum = AnyDist(FileDistribution(filePath)); }
    else throw std::runtime_error("misformatted path to spectrum");
  }
}

std::unique_ptr<BaseSolver> ConfigVisitor::makeSolver()
{
  if ((_alpha.has_value() || _simMode.has_value()) && _fitFile.has_value())
    throw std::runtime_error("config file contains specs for both simulation and fitting!");
  else if (!_alpha.has_value() && !_fitFile.has_value()) throw std::runtime_error("config file missing specs!");

  std::vector<Layer> layers;
  layers.reserve(_layerMap.size());

  for (auto& [key, layer] : _layerMap) { layers.push_back(std::move(layer)); }

  _layerMap.clear();

  std::unique_ptr<BaseSolver> solverPtr;
  if (_alpha.has_value()) {
    solverPtr = std::make_unique<Simulation>(*_simMode, layers, _sweepStart, _sweepStop, *_alpha);
  }
  else {
    solverPtr = std::make_unique<Fitting>(*_fitFile, layers, _sweepStart, _sweepStop);
  }
  return solverPtr;
}

std::unique_ptr<SweepManager> ConfigVisitor::makeSweepManager()
{
  auto smPtr = std::make_unique<SweepManager>();
  // Double dispatch the distributions
  std::visit(
    [&](auto const& x) {
      auto d = dist::as_distribution(x);
      std::visit(
        [&](auto const& y) {
          auto s = dist::as_distribution(y);
          smPtr->setSDSweep(d, s);
        },
        _spectrum.data);
    },
    _dipoleDist.data);

  return smPtr;
}

SolverManager ConfigVisitor::configure()
{
  auto solver = makeSolver();
  auto sm = makeSweepManager();
  return SolverManager(std::move(solver), std::move(sm));
}

bool ConfigVisitor::isSimulation()
{
  if (_alpha.has_value() && _simMode.has_value()) return 1;
  else {
    return 0;
  };
}