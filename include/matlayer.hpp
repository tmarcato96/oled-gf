/*! \file material.hpp
    \brief A header file for the representation of materials
    the material header includes objects created to hold the most important
    characterists of the materials used for the calculations.
*/
#pragma once

#include <complex>
#include <map>
#include <string>
#include <vector>

#include <forwardDecl.hpp>

/*! \class Material
    \brief A class to represent the materials used in the stack.

    The Material class holds the properties of materials. It should be 
    used to represent all the materials contained in the stack. The class
    provides built-in interpolation to allow for the estimation of properties
    that vary considerably depending on physical conditions. For instance,
    the class contains a list of permittivities for the corresponding material, 
    such that interpolation is used to estimate and return the permittivity
    for a given wavelength.
*/

class Material
{
private:
  std::map<double, std::complex<double>> mRefIndices;


public:
  Material();
  
  Material(double realRefIndex, double imagRefIndex);
  /*!< Constructor for the case that the refractive index is not given as a function of the wavelength but just a singular value.*/
  Material(double wavelength, double realRefIndex, double imagRefIndex);
  /*!< Constructor for the case that the refractive index is given for only one specified wavelength.*/
  Material(const std::string& path, const char delimiter = '\t');
  /*!< Constructor for the case of a three column file with wavelength as the first column, followed by the real and imaginary parts of the refractive index, respectively.*/

  std::complex<double> getRefIndex(double wavelength) const;
  /*!< Returns the complex refractive index of the material.*/
  std::complex<double> getEpsilon(double wavelength) const;
  /*!< Returns the complex permittivity of the material.*/
  void insert(double wavelength, double realRefIndex, double imagRefIndex);
};

class Layer {
  Material _material;
  double _thickness;

  public:
      Layer(Material material, double thickness, bool emitterFlag = false);

      const Material& getMaterial() const;
      const double getThickness() const;

      bool isEmitter;
};