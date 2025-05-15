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

                double alpha;
                Vector u;
                Matrix powerUpPerp;
                Matrix powerUpPara;
                Matrix powerUsPara;

        };

        class Exporter{

                void makeTree();

                Results _results;
                Json::JsonNode<Json::PrintVisitor> _root;
                std::shared_ptr<Json::PrintVisitor> _visitor;

                public:

                Exporter(const BaseSolver& solver, std::ostream& sout= std::cout);
                ~Exporter() = default;

                void print();
        };
}