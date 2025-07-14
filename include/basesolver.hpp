/*! \file basesolver.hpp
    \brief A header file which contains the essential calculations needed for both simulating and fitting.

    The basesolver file contains the basesolver class and related structs that perform the fundamental computations
    needed in order to both fit and simulate the behavior of the stack in question. Its main components are the
    BaseSolver class and the coefficient structs used for the calculations performed by BaseSolver and its child
    classes (Simulation and Fitting).
!*/
#pragma once

#include <string>
#include <vector>
#include <sstream>

#include <Eigen/Core>

#include <forwardDecl.hpp>
#include <matlayer.hpp>
#include <polymap.hpp>

struct Distribution
{ // Linear distribution

  double lBound;
  double hBound;
  Vector values;

  Distribution(double xLeft, double xRight, size_t numPoints = 20);
  Distribution(double value);
  Distribution() = default;
};

struct NormalDistribution : public Distribution
{

  NormalDistribution(double xmin, double xmax, double x0, double sigma, size_t NumPoints = 20);
};

template<typename DistType> struct Spectrum
{

public:
  Matrix spectrum;
  DistType distr;

  Spectrum(DistType dist) :
    distr(std::move(dist))
  {
    make_spectrum();
  }

  Spectrum() = default;

protected:
  void make_spectrum()
  {
    size_t numPoints = distr.values.size();
    if (numPoints > 1) {
      spectrum.resize(numPoints, 2);
      spectrum.col(0) = distr.values;
      spectrum.col(1) = Eigen::ArrayXd::LinSpaced(numPoints, distr.lBound, distr.hBound);
    }
    else spectrum = Matrix::Constant(1, 2, distr.values(0));
  }
};

//! A Struct to contain all the Green's Function coefficients.
struct SolverCoefficients
{
  CMatrix _Rperp{};
  CMatrix _Rpara{};
  CMatrix _cb{};
  CMatrix _fb{};
  CMatrix _ct{};
  CMatrix _ft{};
  CMatrix _c{};
  CMatrix _cd{};
  CMatrix _f_perp{};
  CMatrix _fd_perp{};
  CMatrix _f_para{};
  CMatrix _fd_para{};
};

//!  The BaseSolver (virtual) class.
/*!
  The BaseSolver virtual class performs the basic crucial calculations needed for both simulation and fitting.
  It is constructed from the properties of the stack in question, namely, a vector of (class) Material instances,
  a vector of layer thicknesses, the index of the dipole layer, and its position in the stack; and from the wavelength
  of interest.
  This class provides a common interface for interacting with the fitting and simulation classes. It is meant as a
  virtual base class and thus cannot (and should not) be instatiated directly.
*/
class BaseSolver
{
protected:
  BaseSolver(const std::vector<Layer>& Layer,
    const double dipolePosition,
    Spectrum<Distribution> spectrum,
    const double sweepStart,
    const double sweepStop,
    const double alpha = 1.0 / 3.0);

  BaseSolver(const std::vector<Layer>& layers,
    const Distribution& dipoleDist,
    Spectrum<Distribution> spectrum,
    const double sweepStart,
    const double sweepStop,
    const double alpha = 1.0 / 3.0);

  std::vector<Layer> layers;
  Eigen::Index dipoleLayer;
  double dipolePosition;
  double wvl;

  Matrix _spectrum;
  Vector _dipolePositions;
  double _sweepStart;
  double _sweepStop;

  //! A struct to represent a stack of materials and its discretization.
  /*! The MatStack struct contains essential information about the stack, such as the distinct points of its
  discretization, the wavector, its components as well as the permittivities of its materials. This information provides
  the basis for the calculations performed by its containing class, BaseSolver.
  */
  struct MatStack
  {

    Eigen::Index numLayers;
    Eigen::Index numInterfaces;
    Eigen::Index numLayersTop;
    Eigen::Index numLayersBottom;
    Eigen::Index numKVectors;

    CVector epsilon;
    Vector z0;

    CVector x;
    CVector dX;

    Vector u;
    Vector dU;

    CVector k;
    CMatrix h;
  };

  MatStack matstack;
  SolverCoefficients coeffs;

  // Discretization
  void loadMaterialData();
  virtual void discretize() = 0;
  virtual void genInPlaneWavevector() = 0;
  virtual void genOutofPlaneWavevector() = 0;

  // Main calculation
  void calculateFresnelCoeffs();
  /*!< Function to calculate the fresnel coefficients as a function of the materials inputted. */
  void calculateGFCoeffRatios();
  /*!< Function to calculate the ratios between the coefficients of the dyadic Green functions for both left and right
   * travelling eigenfunctions.*/
  void calculateGFCoeffs();
  /*!< Function to calculate the coefficients of the dyadic Green functions from the ratios obtained from
   * calculateGFcoeffRatios.*/
  void calculateLifetime(Vector& bPerp, Vector& bPara);
  /*!< Function to calculate the lifetime of the dipole*/
  void calculateDissPower(const double bPerpSum, const double bParaSum);
  /*!< Function to calculate dissipated power at the output. The power is decomposed in its parallel and perpendicular
   * components.*/

  void calculateWithSpectrum();
  void calculateWithDipoleDistribution();
  void calculate();
  /*!< Function that initializes that properly initializes all coefficients and call the other member functions
  sequentially, as needed to obtain the base results needed for both Fitting and Simulation. In particular, the power
  emitted at the output as given by the real part of the Poynting vector's area integral.*/
public:
  void run();

  void calculateEmissionSubstrate(Vector& thetaGlass,
    Vector& powerPerpGlass,
    Vector& powerParapPolGlass,
    Vector& powerParasPolGlass) const;

  using CMPLX = std::complex<double>;

  virtual ~BaseSolver() = default;

  Vector const& getInPlaneWavevector() const;
  Matrix const& getPowerUpPerp() const;
  Matrix const& getPowerUpPara() const;
  Matrix const& getPowerUsPara() const;
  Eigen::Index getDipoleIndex() const;

  double alpha;

  CMatrix powerPerpUpPol;
  CMatrix powerParaUpPol;
  CMatrix powerParaUsPol;

  Matrix fracPowerPerpUpPol;
  Matrix fracPowerParaUpPol;
  Matrix fracPowerParaUsPol;
};