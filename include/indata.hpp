#pragma once

#include <string>
#include <Eigen/Core>
#include "jsonsimplecpp/node.hpp"
#include <forwardDecl.hpp>

namespace Data {
                Matrix loadFromFile(const std::string& filepath, size_t ncols, char delimiter='\t');
}