/*! \file fitting.hpp
    \brief A header file for fitting energy emission from experimental data.

    The fitting header is used to define the fitting class
    and auxiliar functionalities which are used to fit the 
    energy emission behavior of the stack in question using experimental data.
*/
#pragma once

#include <Eigen/Core>
#include <unsupported/Eigen/NonLinearOptimization>
#include <string>
#include <utility>
#include <vector>
#include <map>

#include <basesolver.hpp>
#include <matlayer.hpp>


// Generic functor
//! A templeted struct used to configure the generic properties of the functor used for optimization. 
template<typename _Scalar, int NX = Eigen::Dynamic, int NY = Eigen::Dynamic> 
struct Functor
{
  typedef _Scalar Scalar;
  enum { InputsAtCompileTime = NX, ValuesAtCompileTime = NY };
  typedef Eigen::Matrix<Scalar, InputsAtCompileTime, 1> InputType;
  typedef Eigen::Matrix<Scalar, ValuesAtCompileTime, 1> ValueType;
  typedef Eigen::Matrix<Scalar, ValuesAtCompileTime, InputsAtCompileTime> JacobianType;

  int m_inputs, m_values;

  Functor() :
    m_inputs(InputsAtCompileTime),
    m_values(ValuesAtCompileTime)
  {}
  Functor(int inputs, int values) :
    m_inputs(inputs),
    m_values(values)
  {}

  int inputs() const { return m_inputs; }
  int values() const { return m_values; }
};

//public struct so that the numerical diff struct can access it
/*! \struct ResFunctor
\brief Struct used as the specific functor needed to pass the objective function to the optimization algorithm.
*/
struct ResFunctor : Functor<double> {
  Matrix powerGlass;
  Vector intensities;
  int operator()(const Eigen::VectorXd& x, Eigen::VectorXd& fvec) const;

  int inputs() const;
  int values() const;
};

/*! \struct ResFunctorNumericalDiff
    \brief Struct used to calculate the jacobian for the optimization algorithm.
*/
struct ResFunctorNumericalDiff : Eigen::NumericalDiff<ResFunctor>{};

/*! \class Fitting
    \brief A class that inherits from BaseSolver to fit experimental data.

    The Fitting class is intended to fit energy emission data. It inherits 
    from BaseSolver and thus shares the same base design as the Simulation
    class and includes additional members to implement specific fitting
    functionalities. The fitting is done using the Levenberg-Marquardt algorithm
    from the Eigen library, therefore the fitting process is fast and computationally
    efficient.
*/
class Fitting : public BaseSolver {

  public:
    Fitting(const std::string& fittingFilePath,
            const std::vector<Layer>& layers,
            const double dipolePosition,
            const double wavelength,
            const double sweepStart,
            const double sweepStop);

    Fitting(Matrix& fitData,
            const std::vector<Layer>& layers,
            const double dipolePosition,
            const double wavelength,
            const double sweepStart,
            const double sweepStop);

    Fitting(const std::string& fittingFilePath,
            const std::vector<Layer>& layers,
            const double dipolePosition,
            const std::string& spectrumFile,
            const double sweepStart,
            const double sweepStop);

    Fitting(const std::string& fittingFilePath,
            const std::vector<Layer>& layers,
            const double dipolePosition,
            const GaussianSpectrum& spectrum,
            const double sweepStart,
            const double sweepStop);

    Fitting(const Matrix& fitData,
            const std::vector<Layer>& layers,
            const double dipolePosition,
            const GaussianSpectrum& spectrum,
            const double sweepStart,
            const double sweepStop);

    Fitting(const std::string& fittingFilePath,
            const std::vector<Layer>& layers,
            const DipoleDistribution& dipoleDist,
            const double wavelength,
            const double sweepStart,
            const double sweepStop);

    Fitting(const Matrix& fitData,
            const std::vector<Layer>& layers,
            const DipoleDistribution& dipoleDist,
            const double wavelength,
            const double sweepStart,
            const double sweepStop);


    Fitting(const std::string& fittingFilePath,
            const std::vector<Layer>& layers,
            const DipoleDistribution& dipoleDist,
            const GaussianSpectrum& spectrum,
            const double sweepStart,
            const double sweepStop);
    
    Fitting(const Matrix& fitData,
            const std::vector<Layer>& layers,
            const DipoleDistribution& dipoleDist,
            const GaussianSpectrum& spectrum,
            const double sweepStart,
            const double sweepStop);


    /*!< Fitting class constructor, the constructor takes a (std) vector of class Material containing the materials of the stack to be simulated, 
    a (std) vector of layer thicknesses with matching indices, the index of the dipole layer, the dipole position within the stack, the chosen wavelength
    and the experimental data to be used for fitting. */

    ~Fitting() = default;

    Matrix calculateEmissionSubstrate(); //MAKE PRIVATE
    /*!< Member method of Fitting used to simulate the emitted power leaving the substrate. The function simulates parallel and perpendicular components of the power emitted,
    so that it returns an Eigen array where the first and second columnns are the perpendicular and parallel components of the emitted power, repectively.*/ 

    std::pair<Eigen::VectorXd, Eigen::ArrayXd> fitEmissionSubstrate();
    /*!< Member method of Fitting used to fit the emitted power leaving the substrate. The function uses the components of the power emitted simulated by
    calculateEmissionSubstrate() and the experimentally obtained intensities in order to compute the residuals for fitting. It uses the Levenberg-Marquadt algorithm to 
    optimize the fittinng parameters and returns a (std) pair containing an Eigen vector of optimized parameters and the Eigen array of emitted power as a function of angle.*/

  // void plot() override;
    Matrix mIntensityData;

    ResFunctorNumericalDiff mResidual;

  private:

    void init(const std::string& fittingFile);

    void genInPlaneWavevector() override; 
    void genOutofPlaneWavevector() override;
    void discretize() override;

};
