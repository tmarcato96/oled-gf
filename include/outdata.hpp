#pragma once

#include <string>
#include <Eigen/core>
#include <forwardDecl.hpp>

namespace Data {
        
        struct Output{
                double alpha;
                Vector u;
                Matrix PowerUpperp;
                Matrix PowerUppara;
                Matrix PowerUspara;
        };

        int writeToFile(const std::string& filepath, const Output& out, bool verbose, char delimiter='\t');
}