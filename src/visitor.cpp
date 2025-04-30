#include <iostream>
#include <map>
#include <jsonsimplecpp/node.hpp>
#include <jsonsimplecpp/parser.hpp>
#include <complex>

#include <forwardDecl.hpp>
#include <matlayer.hpp>
#include <visitor.hpp>


ConfigVisitor::ConfigVisitor(std::map<int, Layer>& layerMap, std::map<double, double>& fitData, std::optional<double>& alpha) :
  _layerMap(layerMap),
  _fitData{fitData},
  _alpha{alpha},
  _depth{0}
  {}

void ConfigVisitor::operator()(const std::unique_ptr<JsonObject>& mapptr) {
  for(auto it = mapptr->begin(); it != mapptr->end(); it++) {
    _depth++;
    if (it->second == nullptr) _depth > 1 ? _helperStack.push(it->first) : throw std::runtime_error("File format error: empty field!");
    else if (std::holds_alternative<std::string>(it->second->value)) _helperStack.push(std::get<std::string>(it->second->value));
    else if (std::holds_alternative<double>(it->second->value)) _helperStack.push(std::get<double>(it->second->value));
    else {
      _helperStack.push(it->first);
      it->second->traverse();  // go deeper into map or list
    }
  }
}

void ConfigVisitor::operator()(const std::unique_ptr<JsonList>& listptr) {
  _depth++;
  for(auto it = listptr->begin(); it != listptr->end(); it++) {
    if (*it == nullptr) throw std::runtime_error("File format error: empty field!");
    else if (std::holds_alternative<std::string>((*it)->value)) _helperStack.push(std::get<std::string>((*it)->value));
    else if (std::holds_alternative<double>((*it)->value)) _helperStack.push(std::get<double>((*it)->value));
    else {(*it)->traverse();} // go deeper into map or list

    if (_depth == 1) fillBaseField();
  }
  _depth--;
}

bool is_digits_only(const std::string& str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

void ConfigVisitor::fillBaseField() {

  //fills layer
  if (std::holds_alternative<std::string>(_helperStack.top()) && (
  std::get<std::string>(_helperStack.top()).substr(0, 5) == (std::string)"Layer" || 
  std::get<std::string>(_helperStack.top()).substr(0, 5) == (std::string)"layer")) {

    bool layerEmitter;
    double layerThickness;
    Material layerMat;
    size_t layerNum = std::stoi(std::get<std::string>(_helperStack.top()).substr(6,  std::get<std::string>(_helperStack.top()).length()));
    
    _helperStack.pop();
    while(!_helperStack.empty()){
      std::string subfield = std::get<std::string>(return_pop(_helperStack));
        if (subfield == "thickness") layerThickness = std::get<double>(return_pop(_helperStack));
        else if (subfield == "emitter") layerEmitter = static_cast<bool>(std::get<double>(return_pop(_helperStack)));
        else if (subfield == "material") layerMat = fillMaterial();
        else { throw std::runtime_error("wrongly formatted layer information!");}
    }
  }

  //fills fitData
  else if (std::holds_alternative<std::string>(_helperStack.top()) && (
  std::get<std::string>(_helperStack.top()) == (std::string)"fitData" || 
  std::get<std::string>(_helperStack.top()) == (std::string)"FitData")) {

      _helperStack.pop();
      _fitData = fillFitData();
  }

  //fill alpha
  else if (std::holds_alternative<std::string>(_helperStack.top()) && (
  std::get<std::string>(_helperStack.top()) == (std::string)"alpha" || 
  std::get<std::string>(_helperStack.top()) == (std::string)"Alpha")) {

      _helperStack.pop();
      _alpha = std::get<double>(return_pop(_helperStack));
  }
  
  else{throw std::runtime_error("misformatted JSON config file!");}
}