#include <iostream>
#include <string>

#include <forwardDecl.hpp>
#include <Eigen/Core>

//put useful file parsing tools here!

namespace Data{
    Matrix loadFromFile(const std::string& filepath, size_t ncols, char delimiter=',');
}//data namespace shared with indata and outdata