#pragma once

#include <string>
#include <Eigen/core>
#include <forwardDecl.hpp>
#include <jsonsimplecpp/node.hpp>
#include "BaseSolver.hpp"

namespace Data {
        
        struct Results{

                Results(const BaseSolver& solver);

                std::string vecToString(const Vector vec);
                std::string layerToString(const CMatrix mat, Eigen::Index layerNum);

                Vector u;
                CMatrix powerUpPerp;
                CMatrix powerUpPara;
                CMatrix powerUsPara;

        };

        class Export{

                Json::JsonNode Export::makeTree();
                
                struct Results results;

                public:

                Export(const BaseSolver& solver);
                ~Export() = default;

                Json::JsonNode root;
        };
}