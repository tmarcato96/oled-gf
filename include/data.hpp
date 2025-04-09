#pragma once

#include <string>

#include <forwardDecl.hpp>

namespace Data {

        Matrix loadFromFile(const std::string& filepath, size_t ncols, char delimiter='\t');
};