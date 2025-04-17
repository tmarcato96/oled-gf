#include <fstream>
#include <ostream>
#include <iostream>
#include <format>
#include <sstream>

#include <forwardDecl.hpp>
#include <jsonsimplecpp/node.hpp>
#include <basesolver.hpp>
#include "outdata.hpp"


Data::Results::Results(const BaseSolver& solver) :
  alpha{solver.alpha},
  u{solver.getInPlaneWavevector()},
  powerUpPerp{solver.mFracPowerPerpUpPol},
  powerUpPara{solver.mFracPowerParaUpPol},
  powerUsPara{solver.mFracPowerParaUsPol}
  {}

std::string Data::Results::vecToString(const Vector& vec) {
  std::stringstream ss;
  ss << vec;
  return ss.str();
}

std::string Data::Results::layerToString(const Matrix& mat, Eigen::Index layerNum) {
    std::stringstream ss;
    ss << mat(layerNum, Eigen::all);
    return ss.str();
}

void Data::Exporter::print(std::ostream& sout) {
    _root.print(sout);
}

void Data::Exporter::print() {
    _root.print();
}

Data::Exporter::Exporter(const BaseSolver& solver) :
  _results{solver}
  {makeTree();}

void Data::Exporter::makeTree() {

  std::string layerUpPerp, layerUpPara, layerUsPara, u;
  std::unique_ptr<Json::JsonNode> alphaVal(new Json::JsonNode{_results.alpha});
  std::unique_ptr<Json::JsonObject> rootObj(new Json::JsonObject{std::pair("alpha", std::move(alphaVal))});

  for(Eigen::Index layerIdx = 0; layerIdx < _results.powerUpPerp.rows(); layerIdx++) {
    std::string layer = std::format("layer {}", size_t(layerIdx));

    layerUpPerp = _results.layerToString(_results.powerUpPerp, layerIdx);
    layerUpPara = _results.layerToString(_results.powerUpPara, layerIdx);
    layerUsPara = _results.layerToString(_results.powerUsPara, layerIdx);

    std::unique_ptr<Json::JsonNode> leafUpPerp = std::make_unique<Json::JsonNode>();
    leafUpPerp->value = layerUpPerp;
    std::unique_ptr<Json::JsonNode> leafUpPara = std::make_unique<Json::JsonNode>();
    leafUpPara->value = layerUpPara;
    std::unique_ptr<Json::JsonNode> leafUsPara = std::make_unique<Json::JsonNode>();
    leafUsPara->value = layerUsPara;

    std::unique_ptr<Json::JsonObject> child = std::make_unique<Json::JsonObject>();
    child->insert(std::pair<std::string, std::unique_ptr<Json::JsonNode>>("PowerUpPerp", std::move(leafUpPerp)));
    child->insert(std::pair<std::string, std::unique_ptr<Json::JsonNode>>("PowerUpPara", std::move(leafUpPara)));
    child->insert(std::pair<std::string, std::unique_ptr<Json::JsonNode>>("PowerUsPara", std::move(leafUsPara)));

    std::unique_ptr<Json::JsonNode> childptr = std::make_unique<Json::JsonNode>();
    childptr->value = std::move(child);
    rootObj->insert(std::pair<std::string, std::unique_ptr<Json::JsonNode>>(layer, std::move(childptr)));
  }
  _root.value = std::move(rootObj);
}