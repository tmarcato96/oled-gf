#pragma once

#include <material.hpp>

class Layer {
    Material _material;
    double _thickness;

    public:
        Layer(Material material, double thickness, bool emitterFlag = false);

        const Material& getMaterial() const;
        const double getThickness() const;

        bool isEmitter;
};