#include <fstream>
#include <ostream>
#include <iostream>
#include <format>
#include <sstream>

#include <forwardDecl.hpp>
#include <jsonsimplecpp/node.hpp>
#include <basesolver.hpp>
#include <outdata.hpp>

using JsonNode = Json::JsonNode<Json::PrintVisitor>;
using JsonObject = std::map<std::string, std::unique_ptr<JsonNode>>;
using JsonList = std::vector<std::unique_ptr<JsonNode>>;

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

void Data::Exporter::print() {
    _root.print();
}

Data::Exporter::Exporter(const BaseSolver& solver, std::ostream& sout) :
  _results{solver},
  _visitor{new Json::PrintVisitor(sout)}
  {makeTree();}

void Data::Exporter::makeTree() {

  std::string layerUpPerp, layerUpPara, layerUsPara, u;
  std::unique_ptr<JsonNode> alphaVal(new JsonNode{_results.alpha, _visitor});
  std::unique_ptr<JsonObject> rootObj(new JsonObject());
  (*rootObj)["alpha"] = std::move(alphaVal);

  for(Eigen::Index layerIdx = 0; layerIdx < _results.powerUpPerp.rows(); layerIdx++) {
    std::string layer = std::format("layer {}", size_t(layerIdx));

    layerUpPerp = _results.layerToString(_results.powerUpPerp, layerIdx);
    layerUpPara = _results.layerToString(_results.powerUpPara, layerIdx);
    layerUsPara = _results.layerToString(_results.powerUsPara, layerIdx);

    std::unique_ptr<JsonNode> leafUpPerp = std::make_unique<JsonNode>();
    leafUpPerp->value = layerUpPerp;
    leafUpPerp->setVisitor(_visitor);
    std::unique_ptr<JsonNode> leafUpPara = std::make_unique<JsonNode>();
    leafUpPara->value = layerUpPara;
    leafUpPara->setVisitor(_visitor);
    std::unique_ptr<JsonNode> leafUsPara = std::make_unique<JsonNode>();
    leafUsPara->value = layerUsPara;
    leafUsPara->setVisitor(_visitor);

    std::unique_ptr<JsonObject> child = std::make_unique<JsonObject>();
    child->insert(std::pair<std::string, std::unique_ptr<JsonNode>>("PowerUpPerp", std::move(leafUpPerp)));
    child->insert(std::pair<std::string, std::unique_ptr<JsonNode>>("PowerUpPara", std::move(leafUpPara)));
    child->insert(std::pair<std::string, std::unique_ptr<JsonNode>>("PowerUsPara", std::move(leafUsPara)));

    std::unique_ptr<JsonNode> childptr = std::make_unique<JsonNode>();
    childptr->value = std::move(child);
    childptr->setVisitor(_visitor);
    rootObj->insert(std::pair<std::string, std::unique_ptr<JsonNode>>(layer, std::move(childptr)));
  }
  _root.value = std::move(rootObj);
  _root.setVisitor(_visitor);
}