#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>

#include <fileutils.hpp>
#include <forwardDecl.hpp>
#include <Eigen/Core>

Matrix Data::loadFromFile(const std::string& filepath, size_t ncols, char delimiter=',') {
  
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