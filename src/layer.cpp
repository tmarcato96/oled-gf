#include <utility>

#include <layer.hpp>

Layer::Layer(Material material, double thickness, bool emitterFlag):
             _material(std::move(material)),
             _thickness(thickness),
             isEmitter(emitterFlag)
{}

const Material& Layer::getMaterial() const {
    return _material;
}

const double Layer::getThickness() const {
    return _thickness;
}