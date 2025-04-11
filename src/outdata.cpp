#include <fstream>
#include <iostream>
#include <sstream>

#include <forwardDecl.hpp>
#include <jsonsimplecpp/node.hpp>
#include "outdata.hpp"


Data::Results::Results(const BaseSolver& solver) :
  u{solver.getInPlaneWavevector()},
  powerUpPerp{solver.mPowerPerpUpPol},
  powerUpPara{solver.mPowerParaUpPol},
  powerUsPara{solver.mPowerParaUsPol}
  {}

std::string Data::Results::vecToString(const Vector vec) {
  std::stringstream ss;
  ss << vec;
  return ss.str();
}

std::string Data::Results::layerToString(const CMatrix mat, Eigen::Index layerNum) {
    std::stringstream ss;
    ss << mat(layerNum, Eigen::all);
    return ss.str();
}

Data::Export::Export(const BaseSolver& solver) :
  results{solver}
  {}

Json::JsonNode Data::Export::makeTree() {
  std::string layerUpPerp, layerUpPara, layerUsPara, u;
  
  for(Eigen::Index layerIdx = 0; layerIdx < results.powerUpPerp.rows(); layerIdx++) {
    layerUpPerp = results.layerToString(results.powerUpPerp, layerIdx);
  }
  
}