#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <cctype>

#include <jsonsimplecpp\node.hpp>
#include <jsonsimplecpp\parser.hpp>

#include <basesolver.hpp>
#include "indata.hpp"

Matrix Data::loadFromFile(const std::string& filepath, size_t ncols, char delimiter) {
  
  std::filesystem::path path(filepath);
  std::ifstream iFile(path);

  if (!iFile.is_open()) { throw std::runtime_error(("Error while opening file " + path.string()).c_str()); }

  std::string line;
  int nlines = 0;
  std::vector<size_t> cols;
  std::vector<double> data;
  bool startOfDataFound = false;

  while (std::getline(iFile, line)) {
    // Skip header lines, i.e. if they start with non digit values
    if (!(startOfDataFound)) {
      if (!(line.empty()) && std::isdigit(line[0])) { startOfDataFound = true; }
      else continue;
    }
    std::istringstream iss(line);
    ++nlines;
    std::string token;
    size_t col = 0;
    while (std::getline(iss, token, delimiter)) {
      try {
        data.push_back(std::stod(token));
        col++;
      } catch (const std::invalid_argument& e) {
        std::cerr << "Conversion error: " << e.what() << "for token: " << token << '\n';
    }

    //if (mMode == 0) {mSimulationData.insert(std::pair{data[MAX_COLS - 3], std::complex<double>{data[MAX_COLS - 2], data[MAX_COLS - 1]}});}
    //else {mFittingData.insert(std::pair{data[MAX_COLS - 3], data[MAX_COLS - 1]});}
    }
    cols.push_back(col);
  }
  if (iFile.bad()) std::perror(("Error while reading the file " + path.string()).c_str());
  // Make sure that all the columns have same number of values!
  if (!(std::all_of(cols.begin(), cols.end(), [first = cols.front()](size_t value) { return value == first; }))) {
    throw std::runtime_error("Material file cannot be parsed. Columns have different number of lines");
  }
  //if (!(std::all_of(cols.begin(), cols.end(), [ncols](size_t value) { return value == ncols; }))) {
  //  throw std::runtime_error("Material file cannot be parsed. Number of columns must be 3");
  //}
  iFile.close();

  Matrix ret = Eigen::Map<Matrix, 0, Eigen::Stride<1, Eigen::Dynamic>>(&data[0], 
                                                                       nlines, 
                                                                       ncols, 
                                                                       Eigen::Stride<1, Eigen::Dynamic>(1, ncols));
  return ret;
}

//import manager stuff
Data::ImportManager::ImportManager(const std::string&filepath) :
  _filepath{filepath},
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
    else if (c = '{') {_ftype = FileFormat::JSON; return;}
  }

  if (_fin.eof()) {
    throw std::runtime_error("Failed to auto-detect file format!");
  } 
  
  else if (_fin.fail()) {
      throw std::runtime_error("File read error occurred when detecting format!");
  }
}

std::unique_ptr<Data::Importer> Data::ImportManager::makeImporter() {
  
  if (_fin.is_open()) _fin.close();

  if (_ftype == FileFormat::JSON) std::unique_ptr<Data::Importer> importer_ptr(new Data::JSONimporter(_filepath));
}

//importer stuff

Data::Importer::Importer(const std::string& filepath) :
  _filepath{filepath},
  _fin{filepath}
  {
    setSolverMode();
  }

  Data::Importer::Importer(const std::string& filepath, Data::SolverMode mode) :
  _filepath{filepath},
  _fin{filepath},
  _mode{mode}
  {}


Data::JSONimporter::JSONimporter(const std::string& filepath, Data::SolverMode mode) :
  _root{setRoot()},
  Data::Importer::Importer(filepath, mode)
  {}

Data::JSONimporter::JSONimporter(const std::string& filepath) :
  _root{setRoot()},
  Data::Importer::Importer(filepath)
  {
    setSolverMode();
  }

std::unique_ptr<Json::JsonNode<Json::PrintVisitor>> Data::JSONinput::setRoot() {

  Json::JsonParser<Json::PrintVisitor> parser(_filepath);
  parser.parse();

  auto resPtr = const_cast<Json::JsonNode<Json::PrintVisitor>*>(parser.getJsonTree());
  std::unique_ptr<Json::JsonNode<Json::PrintVisitor>> rootPtr(resPtr);

  return std::move(rootPtr);
}

std::unique_ptr<Json::JsonNode<Json::PrintVisitor>> Data::JSONinput::setRoot() {

  Json::JsonParser<Json::PrintVisitor> parser(_filepath);
  parser.parse();

  auto resPtr = const_cast<Json::JsonNode<Json::PrintVisitor>*>(parser.getJsonTree());
  std::unique_ptr<Json::JsonNode<Json::PrintVisitor>> rootPtr(resPtr);

  return std::move(rootPtr);
}
