#include <iostream>
#include <map>
#include <jsonsimplecpp/node.hpp>
#include <jsonsimplecpp/parser.hpp>
#include <complex>
#include <optional>
#include <fstream>
#include <queue>

#include <simulation.hpp>
#include <forwardDecl.hpp>
#include <fileutils.hpp>
#include <matlayer.hpp>
#include <visitor.hpp>


void ConfigVisitor::operator()(const std::unique_ptr<JsonObject>& mapptr) {
  _depth++;
  for(auto it = mapptr->begin(); it != mapptr->end(); it++) {
    if (it->second == nullptr) _depth > 1 ? _helperQueue.push(it->first) : throw std::runtime_error("File format error: empty field!");
    else if (std::holds_alternative<std::string>(it->second->value)) _helperQueue.push(std::get<std::string>(it->second->value));
    else if (std::holds_alternative<double>(it->second->value)) _helperQueue.push(std::get<double>(it->second->value));
    else {
      _helperQueue.push(it->first);
      it->second->traverse();  // go deeper into map
    }
    if (_depth == 1) fillBaseField();
  }
  _depth--;
}

void ConfigVisitor::operator()(const std::unique_ptr<JsonList>& listptr) {
  _depth++;
  for(auto it = listptr->begin(); it != listptr->end(); it++) {
    if (*it == nullptr) throw std::runtime_error("File format error: empty field!");
    else if (std::holds_alternative<std::string>((*it)->value)) _helperQueue.push(std::get<std::string>((*it)->value));
    else if (std::holds_alternative<double>((*it)->value)) _helperQueue.push(std::get<double>((*it)->value));
    else {(*it)->traverse();} // go deeper into list

    if (_depth == 1) fillBaseField();
  }
  _depth--;
}

void ConfigVisitor::fillBaseField() {

  if(!std::holds_alternative<std::string>(_helperQueue.front())) throw std::runtime_error("misformatted JSON config file!");
  std::string field = std::get<std::string>(_helperQueue.front());

  //fills layer
  if (field.substr(0, 5) == static_cast<std::string>("Layer") || 
      field.substr(0, 5) == static_cast<std::string>("layer")) {

    bool layerEmitter;
    double layerThickness;
    Material layerMat;
    size_t layerNum = std::stoi(field.substr(6, field.length()));
    
    _helperQueue.pop();
    while(!_helperQueue.empty()){
      std::string subfield = std::get<std::string>(return_pop(_helperQueue));
        if (subfield == "thickness") layerThickness = std::get<double>(return_pop(_helperQueue));
        else if (subfield == "emitter") layerEmitter = static_cast<bool>(std::get<double>(return_pop(_helperQueue)));
        else if (subfield == "material") fillMaterialHelper(layerMat);
        else { throw std::runtime_error("wrongly formatted layer information!");}
    }

    Layer layer{layerMat, layerThickness, layerEmitter};
    _layerMap.insert(std::pair<int, Layer>{layerNum, layer});
  }

  //fills fitData
  else if (field == static_cast<std::string>("fitData") || 
           field == static_cast<std::string>("FitData")) {

    _helperQueue.pop();
    //for CSV reference
    std::string subfield = std::get<std::string>(return_pop(_helperQueue));
    if (subfield.contains('/') || subfield.contains('\\')) {
  
      _fitData = Data::loadFromFile(subfield, 2);
    }
  
    //for in-file intensities
    else if (std::isdigit(subfield[0])) {
  
      double wavelength = std::stoi(subfield);
      subfield = std::get<std::string>(return_pop(_helperQueue));
      double intensity = std::stoi(subfield.substr(0, subfield.find(',')));
  
      _fitData.value()(_helperFitCalls, 0) = wavelength;  //double check
      _fitData.value()(_helperFitCalls, 1) = intensity;
      _helperFitCalls++;
    }
    else{throw std::runtime_error("misformatted fitData filed in JSON config file!");}
  }

  //fills alpha
  else if (field == static_cast<std::string>("alpha") || 
           field == static_cast<std::string>("Alpha")) {

    _helperQueue.pop();
    _alpha = std::get<double>(return_pop(_helperQueue));
  }
  
  //fills dipole 
  else if (field == static_cast<std::string>("dipole")|| 
           field == static_cast<std::string>("Dipole")) {

    _helperQueue.pop();
    std::string subfield = std::get<std::string>(return_pop(_helperQueue));
    if (subfield ==  static_cast<std::string>("uniform")) fillDipoleModeHelper();
    else{
      _helperQueue.pop();
      _dipoleDist = std::get<double>(return_pop(_helperQueue));
    }
  }

  //fills spectrum
  else if (field == static_cast<std::string>("spectrum")|| 
           field == static_cast<std::string>("Spectrum")) {

    _helperQueue.pop();
    std::string subfield = std::get<std::string>(return_pop(_helperQueue));
    if (subfield ==  static_cast<std::string>("gaussian")) fillSpectrumModeHelper();
    else{
      _helperQueue.pop();
      _spectrum = std::get<double>(return_pop(_helperQueue));
    }
  }

  //fills simulation mode stuff
  else if (field == static_cast<std::string>("simtype")|| 
           field == static_cast<std::string>("Simtype")) {

          _helperQueue.pop();
          std::string subfield = std::get<std::string>(return_pop(_helperQueue));
          if (subfield == "anglesweep") _simMode = SimulationMode::AngleSweep;
          else if (subfield == "modedissipation") _simMode = SimulationMode::ModeDissipation;
  }

  //fills sweep configuration
  else if (field == static_cast<std::string>("sweep")|| 
           field == static_cast<std::string>("Sweep")) {

          _helperQueue.pop();
          std::string subfield = std::get<std::string>(return_pop(_helperQueue));
          if (subfield == "start") _sweepStart = std::get<double> (return_pop(_helperQueue));
          else if (subfield == "stop") _sweepStop = std::get<double> (return_pop(_helperQueue));
          else {throw std::runtime_error("misformatted sweep settings");}
  }
  else{throw std::runtime_error("JSON config file contains ambiguous parameters!");}
}

void ConfigVisitor::fillMaterialHelper(Material& mat) {

  //for CSV reference
  std::string subfield = std::get<std::string>(return_pop(_helperQueue));
  if (subfield.contains('/') || subfield.contains('\\')) {
    Material res(subfield, ',');
    mat = res;
  }

  //for in-file intensities
  else if (std::isdigit(subfield[0])) {

    double wavelength = std::stoi(subfield);

    subfield = std::get<std::string>(return_pop(_helperQueue));
    double realRefIndex = std::stoi(subfield.substr(0, subfield.find(',')));

    subfield.erase(0, subfield.find(','));  //double check
    double imagRefIndex = std::stoi(subfield.substr(0, subfield.find(',')));

    mat.insert(wavelength, realRefIndex, imagRefIndex);
  }
  else{throw std::runtime_error("misformatted layer material field in JSON config file!");}
}

void ConfigVisitor::fillDipoleModeHelper() {
  //it doesn't matter if subfield is zmax or zmin for uniform, just fill two doubles and compare
  _helperQueue.pop();
  double z1 = std::get<double>(return_pop(_helperQueue));

  _helperQueue.pop();
  double z2 = std::get<double>(return_pop(_helperQueue));

  z1 > z2 ? DipoleDistribution(z1, z2, DipoleDistributionType::Uniform) : DipoleDistribution(z2, z1, DipoleDistributionType::Uniform);
}

void ConfigVisitor::fillSpectrumModeHelper() {
  
  double xmin;
  double xmax;
  double x0;
  double sigma;

  _helperQueue.pop();
  for (size_t i = 0; i < 4; i++){
    auto subfield = std::get<std::string>(return_pop(_helperQueue));
    auto val = std::get<double>(return_pop(_helperQueue));
    if (subfield == "xmin") xmin = val;
    else if(subfield =="xmax") xmax = val;
    else if (subfield == "x0") x0 = val;
    else if (subfield == "sigma") sigma = val;
    else {throw std::runtime_error("misformatted spectrum info provided!");}
  }
  _spectrum = GaussianSpectrum(xmin, xmax, x0, sigma);
}