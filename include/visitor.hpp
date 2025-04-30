#include <iostream>
#include <map>
#include <stack>
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

  void checkKey();
  ConfigVisitor(std::map<int, Layer>& layerMap, std::map<double, double>& fitData, std::optional<double>& alpha);
  void setStream(std::ostream& stream);

private:
  std::map<int, Layer> _layerMap;
  std::map<double, double> _fitData;
  std::optional<double> _alpha;
  size_t _depth;

  //private helper stuff
  std::stack<ValueType> _helperStack;

  bool is_digits_only(const std::string& str);
  Material fillMaterial();
  std::map<double, double> fillFitData();
  void fillBaseField();

  template <typename T>
  T return_pop(std::stack<T>& s) { //returns the top element and pops stack IN THIS ORDER
    if (s.empty()) throw std::runtime_error("Stack is empty");
    T top = s.top();
    s.pop();
    return top;
  }
};

//note to self: don't forget to differentiate between simulation and fitting!!!!