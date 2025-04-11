#pragma once

#include <string>
#include <Eigen/core>
#include "jsonsimplecpp/node.hpp"
#include <forwardDecl.hpp>

namespace Data {
                Matrix loadFromFile(const std::string& filepath, size_t ncols, char delimiter='\t');
}