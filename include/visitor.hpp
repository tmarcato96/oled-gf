#include <iostream>
#include <map>
#include <queue>
#include <filesystem>
#include <fstream>
#include <matlayer.hpp>
#include <jsonsimplecpp/node.hpp>
#include <jsonsimplecpp/parser.hpp>

struct ConfigVisitor {

  using JsonObject = std::map<std::string, std::unique_ptr<Json::JsonNode<ConfigVisitor>>>;
  using JsonList = std::vector<std::unique_ptr<Json::JsonNode<ConfigVisitor>>>;
  using ValueType = std::variant<std::string, double>;

  void operator()(const std::unique_ptr<JsonObject>&);
  void operator()(const std::unique_ptr<JsonList>&);
  void operator()(const std::string&);
  void operator()(double);

  ConfigVisitor(std::map<int, Layer>& layerMap, std::map<double, double>& fitData, std::optional<double>& alpha);

private:
  std::map<int, Layer> _layerMap;
  std::optional<double> _alpha;
  Matrix _fitData;
  size_t _depth;

  //private helper stuff
  std::queue<ValueType> _helperQueue;
  int _helperFitCalls;

  void fillFitData();
  void fillMaterial(Material& mat);
  void fillBaseField();

  template <typename T>
  T return_pop(std::queue<T>& s) { //returns the top element and pops queue IN THIS ORDER
    if (s.empty()) throw std::runtime_error("Stack is empty");
    T first = s.first();
    s.pop();
    return first;
  }
};

//note to self: don't forget to differentiate between simulation and fitting!!!!