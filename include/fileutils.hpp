#pragma once

#include <iostream>
#include <string>

#include <Eigen/Core>
#include <forwardDecl.hpp>

// put useful file parsing tools here!

namespace Data {
  Matrix loadFromFile(const std::string& filepath, size_t ncols, char delimiter = ',');
  void sortRowsByFirstColumn(Matrix& m);
} // namespace Data