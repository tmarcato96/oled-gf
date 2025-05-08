#pragma once

#include <iostream>
#include <map>
#include <queue>
#include <optional>
#include <filesystem>
#include <fstream>
#include <matlayer.hpp>
#include <simulation.hpp>
#include <fitting.hpp>
#include <basesolver.hpp>
#include <jsonsimplecpp/node.hpp>
#include <jsonsimplecpp/parser.hpp>



template <typename T>
T return_pop(std::queue<T>& s) { //returns the top element and pops queue IN THIS ORDER
  if (s.empty()) throw std::runtime_error("Object is empty");
  T first = s.front();
  s.pop();
  return first;
}

struct ConfigVisitor {
  public:
    using JsonObject = std::map<std::string, std::unique_ptr<Json::JsonNode<ConfigVisitor>>>;
    using JsonList = std::vector<std::unique_ptr<Json::JsonNode<ConfigVisitor>>>;
    using ValueType = std::variant<std::string, double>;

    void operator()(const std::unique_ptr<JsonObject>&);
    void operator()(const std::unique_ptr<JsonList>&);
    void operator()(const double);
    void operator()(const std::string&);

    ConfigVisitor()=default;

    std::unique_ptr<BaseSolver> makeSolver();
    bool isSimulation();

  private:
    std::map<int, Layer> _layerMap;
    std::vector<Layer> _layers;
    std::optional<Matrix> _fitData;
    std::optional<double> _alpha;
    std::optional<SimulationMode> _simMode;
    std::variant<DipoleDistribution, double> _dipoleDist;
    std::variant<GaussianSpectrum, double> _spectrum;

  
    double _sweepStart;
    double _sweepStop;
    size_t _depth;

    //private helper stuff
    std::queue<ValueType> _helperQueue;
    int _helperFitCalls;

    void fillBaseField();
    void fillMaterialHelper(Material& mat);
    void fillDipoleModeHelper();
    void fillSpectrumModeHelper();
};

//note to self: don't forget to differentiate between simulation and fitting!!!!