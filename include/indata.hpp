#pragma once

#include <string>
#include <Eigen/core>
#include <forwardDecl.hpp>

namespace Data {
                Matrix loadFromFile(const std::string& filepath, size_t ncols, char delimiter='\t');
}