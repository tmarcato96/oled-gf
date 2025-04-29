#include <iostream>
#include <matlayer.hpp>
#include <jsonsimplecpp/node.hpp>
#include <jsonsimplecpp/parser.hpp>

template<class VisitorPolicy> using JsonObject = std::map<std::string, std::unique_ptr<Json::JsonNode<VisitorPolicy>>>;

template<class VisitorPolicy> using JsonList = std::vector<std::unique_ptr<Json::JsonNode<VisitorPolicy>>>;

struct ConfigVisitor {
  void operator()(const std::unique_ptr<JsonObject<ConfigVisitor>>&);
  void operator()(const std::unique_ptr<JsonList<ConfigVisitor>>&);
  void operator()(const std::string&);
  void operator()(double);

  ConfigVisitor(std::map<int, Layer>& layerMap, std::map<double, double>& fitData, std::optional<double>& alpha);
  void setStream(std::ostream& stream);

private:
  std::map<int, Layer> _layerMap;
  std::map<double, double> _fitData;
  std::optional<double> _alpha;
  size_t _depth;
};