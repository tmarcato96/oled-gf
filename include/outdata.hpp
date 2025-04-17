#pragma once

#include <string>
#include <ostream>
#include <Eigen/Core>
#include <forwardDecl.hpp>
#include <jsonsimplecpp/node.hpp>
#include "basesolver.hpp"

namespace Data {
        
        struct Results{

                Results(const BaseSolver& solver);

                std::string vecToString(const Vector& vec);
                std::string layerToString(const Matrix& mat, Eigen::Index layerNum);

                Vector u;
                Matrix powerUpPerp;
                Matrix powerUpPara;
                Matrix powerUsPara;

        };

        class Exporter{

                void makeTree();
        
                Results _results;
                Json::JsonNode _root;

                public:

                Exporter(const BaseSolver& solver);
                ~Exporter() = default;

                void print(std::ostream& sout);
                void print();
        };
}