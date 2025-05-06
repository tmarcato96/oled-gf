#include <iostream>
#include <map>
#include <jsonsimplecpp/node.hpp>
#include <jsonsimplecpp/parser.hpp>
#include <complex>
#include <fstream>
#include <queue>

#include <forwardDecl.hpp>
#include <fileutils.hpp>
#include <matlayer.hpp>
#include <visitor.hpp>


ConfigVisitor::ConfigVisitor(std::map<int, Layer>& layerMap, std::map<double, double>& fitData, std::optional<double>& alpha) :
  _layerMap(layerMap),
  _fitData{fitData},
  _alpha{alpha},
  _helperQueue{},
  _helperFitCalls{0},
  _depth{0}
  {}

void ConfigVisitor::operator()(const std::unique_ptr<JsonObject>& mapptr) {
  for(auto it = mapptr->begin(); it != mapptr->end(); it++) {
    _depth++;
    if (it->second == nullptr) _depth > 1 ? _helperQueue.push(it->first) : throw std::runtime_error("File format error: empty field!");
    else if (std::holds_alternative<std::string>(it->second->value)) _helperQueue.push(std::get<std::string>(it->second->value));
    else if (std::holds_alternative<double>(it->second->value)) _helperQueue.push(std::get<double>(it->second->value));
    else {
      _helperQueue.push(it->first);
      it->second->traverse();  // go deeper into map or list
    }
  }
}

void ConfigVisitor::operator()(const std::unique_ptr<JsonList>& listptr) {
  _depth++;
  for(auto it = listptr->begin(); it != listptr->end(); it++) {
    if (*it == nullptr) throw std::runtime_error("File format error: empty field!");
    else if (std::holds_alternative<std::string>((*it)->value)) _helperQueue.push(std::get<std::string>((*it)->value));
    else if (std::holds_alternative<double>((*it)->value)) _helperQueue.push(std::get<double>((*it)->value));
    else {(*it)->traverse();} // go deeper into map or list

    if (_depth == 1) fillBaseField();
  }
  _depth--;
}

void ConfigVisitor::fillBaseField() {

  //fills layer
  if (std::holds_alternative<std::string>(_helperQueue.front()) && (
  std::get<std::string>(_helperQueue.front()).substr(0, 5) == (std::string)"Layer" || 
  std::get<std::string>(_helperQueue.front()).substr(0, 5) == (std::string)"layer")) {

    bool layerEmitter;
    double layerThickness;
    Material layerMat;
    size_t layerNum = std::stoi(std::get<std::string>(_helperQueue.front()).substr(6,  std::get<std::string>(_helperQueue.front()).length()));
    
    _helperQueue.pop();
    while(!_helperQueue.empty()){
      std::string subfield = std::get<std::string>(return_pop(_helperQueue));
        if (subfield == "thickness") layerThickness = std::get<double>(return_pop(_helperQueue));
        else if (subfield == "emitter") layerEmitter = static_cast<bool>(std::get<double>(return_pop(_helperQueue)));
        else if (subfield == "material") fillMaterial(layerMat);
        else { throw std::runtime_error("wrongly formatted layer information!");}
    }

    Layer layer{layerMat, layerThickness, layerEmitter};
    _layerMap.insert(std::pair<int, Layer>{layerNum, layer});
  }

  //fills fitData
  else if (std::holds_alternative<std::string>(_helperQueue.front()) && (
  std::get<std::string>(_helperQueue.front()) == (std::string)"fitData" || 
  std::get<std::string>(_helperQueue.front()) == (std::string)"FitData")) {

    _helperQueue.pop();
    fillFitData();
  }

  //fill alpha
  else if (std::holds_alternative<std::string>(_helperQueue.front()) && (
  std::get<std::string>(_helperQueue.front()) == (std::string)"alpha" || 
  std::get<std::string>(_helperQueue.front()) == (std::string)"Alpha")) {

    _helperQueue.pop();
    _alpha = std::get<double>(return_pop(_helperQueue));
  }
  
  else{throw std::runtime_error("misformatted JSON config file!");}
}

void ConfigVisitor::fillMaterial(Material& mat) {

  //for CSV reference
  std::string subfield = std::get<std::string>(return_pop(_helperQueue));
  if (subfield.contains('/') || subfield.contains('\\')) {
    Material res(subfield, ',');
    mat = res;
  }

  //for in-file intensities
  else if (std::isdigit(subfield[0])) {

    double wavelength = std::stoi(subfield);

    std::string subfield = std::get<std::string>(return_pop(_helperQueue));
    double realRefIndex = std::stoi(subfield.substr(0, subfield.find(',')));

    subfield.erase(0, subfield.find(','));  //double check
    double imagRefIndex = std::stoi(subfield.substr(0, subfield.find(',')));

    mat.insert(wavelength, realRefIndex, imagRefIndex);
  }
  
  else{throw std::runtime_error("misformatted layer material field in JSON config file!");}
}

void ConfigVisitor::fillFitData() {
  //for CSV reference
  std::string subfield = std::get<std::string>(return_pop(_helperQueue));
  if (subfield.contains('/') || subfield.contains('\\')) {

    _fitData = Data::loadFromFile(subfield, 2);
  }

  //for in-file intensities
  else if (std::isdigit(subfield[0])) {

    double wavelength = std::stoi(subfield);
    std::string subfield = std::get<std::string>(return_pop(_helperQueue));
    double intensity = std::stoi(subfield.substr(0, subfield.find(',')));

    _fitData(_helperFitCalls, 0) = wavelength;
    _fitData(_helperFitCalls, 1) = intensity;
    _helperFitCalls++;
  }
  else{throw std::runtime_error("misformatted fitData filed in JSON config file!");}
}

