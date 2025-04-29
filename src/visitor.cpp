#include <iostream>
#include <jsonsimplecpp/node.hpp>
#include <jsonsimplecpp/parser.hpp>

#include <visitor.hpp>


ConfigVisitor::ConfigVisitor(std::map<int, Layer>& layerMap, std::map<double, double>& fitData, std::optional<double>& alpha) :
  _layerMap(layerMap),
  _fitData{fitData},
  _alpha{alpha},
  _depth{0}
  {}

void ConfigVisitor::operator()(const std::unique_ptr<JsonObject<ConfigVisitor>>& objectptr) {
    for(auto it = objectptr->begin(); it != objectptr->end(); it++){
        if(it->second == nullptr){ }
    } 


}

void ConfigVisitor::operator()(const std::unique_ptr<JsonList<ConfigVisitor>>& listptr) {
  *_out_stream << "[";
  for (auto it = list->begin(); it != list->end(); ++it) {
    std::visit(*this, (*it)->value);
    auto next = it;
    ++next;
    if (next != list->end()) { *_out_stream << ", "; }
  }
  *_out_stream << "]";
}

void ConfigVisitor::operator()(const std::string& s) { *_out_stream << '"' << s << '"'; }

void ConfigVisitor::operator()(double num) { *_out_stream << num; }