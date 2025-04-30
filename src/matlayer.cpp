#include "matlayer.hpp"
#include "indata.hpp"

#include <utility>
#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>
#include <stdexcept>
#include <Eigen/Core>


Material::Material() :
  mRefIndices{std::pair{0.0, 0.0}}
  {}

Material::Material(double realRefIndex, double imagRefIndex)
{
  Eigen::ArrayXd wavelength = Eigen::ArrayXd::LinSpaced(50, 300, 800);
  for (auto wvl: wavelength) { 
    mRefIndices.insert(std::pair<double, std::complex<double>>{wvl, std::complex<double>(realRefIndex, imagRefIndex)});
  }
}

Material::Material(double wavelength, double realRefIndex, double imagRefIndex)
{
  mRefIndices.insert(std::pair{wavelength, std::complex<double>(realRefIndex, imagRefIndex)});
}

Material::Material(const std::string& path, const char delimiter)
{
  Matrix data = Data::loadFromFile(path, 3, delimiter);
  for (Eigen::Index i=0; i<data.rows(); ++i) {
    mRefIndices.insert(std::pair<double, std::complex<double>>{data(i, 0), 
                                                               std::complex<double>(data(i, 1),
                                                                                    data(i, 2))});
  }
}

std::complex<double> Material::getRefIndex(double wavelength) const
{

  std::complex<double> res;
  try {
    res = mRefIndices.at(wavelength);
    return res;
  } catch (const std::out_of_range& oor) {
    auto const uBound = mRefIndices.upper_bound(wavelength);
    if (uBound != mRefIndices.end()) {
      auto const lBound = std::prev(uBound);
      double ratio = (wavelength - lBound->first) / (uBound->first - lBound->first);
      res = ratio * uBound->second + (1 - ratio) * lBound->second; // Interpolation
      return res;
    }
    else throw std::runtime_error("The wavelength provided is outside the range of the material data");
  }
}

std::complex<double> Material::getEpsilon(double wavelength) const
{
  std::complex<double> res = getRefIndex(wavelength);
  return res * res;
}

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