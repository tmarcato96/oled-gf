#include <exception>
#include <iostream>
#include <optional>
#include <sstream>
#include <cctype>

#include <jsonsimplecpp\node.hpp>
#include <jsonsimplecpp\parser.hpp>

#include <basesolver.hpp>
#include <fitting.hpp>
#include <simulation.hpp>
#include <visitor.hpp>
#include <indata.hpp>


//import manager stuff
Data::ImportManager::ImportManager(const std::string& filepath) :
  _filepath{std::move(filepath)},
  _fin{filepath},
  _smode{SolverMode::automatic}
  {
    autoSetFileFormat();
  }

Data::ImportManager::ImportManager(const std::string&filepath, Data::SolverMode smode) :
  _filepath{filepath},
  _fin{filepath},
  _smode{smode} 
  {
    autoSetFileFormat();
  }

Data::ImportManager::ImportManager(const std::string&filepath, Data::FileFormat ftype) :
  _filepath{filepath},
  _fin{filepath},
  _ftype{ftype},
  _smode{SolverMode::automatic}
  {}

Data::ImportManager::ImportManager(const std::string& filepath, Data::FileFormat ftype, Data::SolverMode smode) :
  _filepath{filepath},
  _fin{filepath},
  _ftype{ftype},
  _smode{smode}
  {}

void Data::ImportManager::autoSetFileFormat() {
  
  if (!_fin) {
    throw std::runtime_error("Failed to open file!");
  }

  char c;
  while (_fin.get(c)) {
    if (std::isalnum(static_cast<unsigned char>(c))) {_ftype = FileFormat::CSV; return;}
    else if (c == '{') {_ftype = FileFormat::JSON; return;}
  }

  if (_fin.eof()) {
    throw std::runtime_error("Failed to auto-detect file format!");
  } 
  
  else if (_fin.fail()) {
    throw std::runtime_error("File read error occurred when detecting format!");
  }
}

std::unique_ptr<Data::Importer> Data::ImportManager::makeImporter() {
  std::unique_ptr<Data::Importer> importer_ptr;
  if (_fin.is_open()) _fin.close();
  if (_ftype == FileFormat::JSON) importer_ptr = std::make_unique<Data::JSONimporter>(_filepath);
  else{throw std::runtime_error("support for other file formats not implemented et!");}
  return importer_ptr;
}

//importer stuff

Data::Importer::Importer(const std::string& filepath) :
  _filepath{filepath}
  {}

Data::Importer::Importer(const std::string& filepath, Data::SolverMode mode) :
  _filepath{filepath},
  _mode{mode}
  {}


Data::JSONimporter::JSONimporter(const std::string& filepath, Data::SolverMode mode) :
  Data::Importer::Importer(filepath, mode),
  _reader{filepath}
  {}

Data::JSONimporter::JSONimporter(const std::string& filepath) :
  Data::Importer::Importer(filepath),
  _reader{filepath}
  {
    setSolverMode();
  }

void  Data::JSONimporter::setSolverMode() {
  _mode = _reader.get_mode();
}

std::unique_ptr<BaseSolver> Data::JSONimporter::solverFromFile() {
  return _reader.makeSolver();
}