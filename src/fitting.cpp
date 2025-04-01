#define _USE_MATH_DEFINES
#include <Eigen/Core>
#include <unsupported/Eigen/NonLinearOptimization>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>
#include <matplot/matplot.h>

#include "data.hpp"
#include "fitting.hpp"
#include "linalg.hpp"
#include "material.hpp"
#include "basesolver.hpp"


Fitting::Fitting(const std::string& fittingFilePath,
                 const std::vector<Layer>& layers,
                 const double dipolePosition,
                 const double wavelength,
                 const double sweepStart,
                 const double sweepStop):
                 BaseSolver(SimulationMode::AngleSweep,
                 layers,
                 dipolePosition,
                 wavelength,
                 sweepStart,
                 sweepStop),
                 _fittingFile(std::move(fittingFilePath))
                  {
                    init();
}

Fitting::Fitting(const std::string& fittingFilePath,
                 const std::vector<Layer>& layers,
                 const double dipolePosition,
                 const std::string& spectrumFile,
                 const double sweepStart,
                 const double sweepStop):
                 BaseSolver(SimulationMode::AngleSweep,
                            layers,
                            dipolePosition,
                            spectrumFile,
                            sweepStart,
                            sweepStop),
                 _fittingFile(std::move(fittingFilePath)) {
                  init();
}

Fitting::Fitting(const std::string& fittingFilePath,
                 const std::vector<Layer>& layers,
                 const double dipolePosition,
                 const GaussianSpectrum& spectrum,
                 const double sweepStart,
                 const double sweepStop):
                 BaseSolver(SimulationMode::AngleSweep,
                            layers,
                            dipolePosition,
                            spectrum,
                            sweepStart,
                            sweepStop),
                 _fittingFile(std::move(fittingFilePath)) {
                  init();
}

Fitting::Fitting(const std::string& fittingFilePath,
                 const std::vector<Layer>& layers,
                 const DipoleDistribution& dipoleDist,
                 const GaussianSpectrum& spectrum,
                 const double sweepStart,
                 const double sweepStop):
                 BaseSolver(SimulationMode::AngleSweep,
                            layers,
                            dipoleDist,
                            spectrum,
                            sweepStart,
                            sweepStop),
                 _fittingFile(std::move(fittingFilePath)) {
                  init();
}

void Fitting::init() {
  // Log initialization of Simulation
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Initializing Fitting             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";
  mIntensityData = Data::loadFromFile(_fittingFile, 2);
  this->discretize();
  run();
  //setting up functor for fitting
  mResidual.intensities = mIntensityData.col(1);
  mResidual.powerGlass = calculateEmissionSubstrate();
}

void Fitting::genInPlaneWavevector() {
  // Cumulative sum of thicknesses
  matstack.z0.resize(matstack.numLayers - 1);
  matstack.z0(0) = 0.0;
  std::vector<double> thicknesses;
  for (size_t i=1; i < mLayers.size()-1; ++i) {
    thicknesses.push_back(mLayers[i].getThickness());
  }
  std::partial_sum(thicknesses.begin(), thicknesses.end(), std::next(matstack.z0.begin()), std::plus<double>());
  matstack.z0 -= (matstack.z0(mDipoleLayer - 1) + mDipolePosition);

  //getting sim data
//QUADRUPLE CHECK THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
  Vector u_range = Eigen::real(Eigen::sqrt(matstack.epsilon(matstack.numLayers - 1)/matstack.epsilon(mDipoleLayer)*(1- pow(Eigen::cos(mIntensityData.col(0)), 2))));
  matstack.u = u_range.head(u_range.size()-1);
  matstack.x = matstack.u.acos();
  matstack.numKVectors = matstack.u.size();
  
  // Differences
  matstack.dU = u_range.segment(1, u_range.size() - 1) - u_range.segment(0, u_range.size() - 1);
  matstack.dX = u_range.segment(1, u_range.size() - 1).acos() - u_range.segment(0, u_range.size() - 1).acos();
}

void Fitting::genOutofPlaneWavevector() {
  // Out of plane wavevector
  matstack.k = 2 * M_PI / mWvl / 1e-9 * matstack.epsilon.sqrt();
  matstack.h.resize(matstack.numLayers, matstack.u.size());
  matstack.h = matstack.k(mDipoleLayer) *
               (((matstack.epsilon.replicate(1, matstack.x.size())) / matstack.epsilon(mDipoleLayer)).rowwise() -
                 matstack.u.pow(2).transpose()).sqrt();
}

void Fitting::discretize() {
  loadMaterialData();
  genInPlaneWavevector();
  genOutofPlaneWavevector();
}

Matrix Fitting::calculateEmissionSubstrate() {
  Vector powerPerppPolGlass;
  Vector powerParapPolGlass;

  powerPerppPolGlass = ((Eigen::real(mPowerPerpUpPol(matstack.numLayers - 2, Eigen::all))) *
                    std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(mDipoleLayer))));
  powerPerppPolGlass /= Eigen::tan(mIntensityData.col(0));

  powerParapPolGlass = ((Eigen::real(mPowerParaUpPol(matstack.numLayers - 2, Eigen::all))) *
                    std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(mDipoleLayer))));
  powerParapPolGlass /= Eigen::tan(mIntensityData.col(0));


  Matrix powerGlass(2, powerPerppPolGlass.size());
  powerGlass.row(0) = powerPerppPolGlass;
  powerGlass.row(1) = powerParapPolGlass;
  return powerGlass;
}

int ResFunctor::operator()(const Eigen::VectorXd& x, Eigen::VectorXd& fvec) const {
  // x here is vector of fitting params
  for (size_t i = 0; i < intensities.size(); ++i) {
    fvec(i) = intensities(i) - x(0) * (x(1)*powerGlass(0, i) + (1 - x(1))*powerGlass(1, i)); // residual of each sample
  }
  return 0;
} 

int ResFunctor::inputs() const {return 2;}

int ResFunctor::values() const {return intensities.size();}

std::pair<Eigen::VectorXd, Eigen::ArrayXd> Fitting::fitEmissionSubstrate() {
  //returns the vector of parameters and the fitted intensities as a std::pair

  std::vector<double> theta(matstack.x.rows()), yFit(matstack.x.rows()), yExp(mResidual.intensities.rows());


  Eigen::ArrayXd::Map(&theta[0], mIntensityData.rows()) = mIntensityData.col(0);
  Eigen::ArrayXd::Map(&yExp[0], mIntensityData.rows()) = mResidual.intensities;

  // Setup
  Eigen::VectorXd x(2);
  // Initial guess
  x(0) = 1.0;
  x(1) = 0.19;

  Eigen::LevenbergMarquardt<ResFunctorNumericalDiff> lm(mResidual);
  lm.parameters.maxfev = 2000;
  lm.parameters.xtol = 1e-10;
  lm.parameters.ftol = 1e-10;

  int status = lm.minimize(x);
  std::cout << "Number of iterations: " << lm.iter << '\n';
  std::cout << "Status: " << status << '\n';
  std::cout << "Fitting result: " << x << '\n' << '\n';

  // simulation results
  Eigen::ArrayXd optIntensities(matstack.x.rows());
  optIntensities = x(0) * (x(1)*mResidual.powerGlass.row(0) + (1 - x(1))*mResidual.powerGlass.row(1));
  Eigen::ArrayXd::Map(&yFit[0], matstack.x.rows()) = optIntensities;

  //plotting the results
  matplot::figure();
  
  matplot::scatter(theta, yExp);
  matplot::hold(matplot::on);
  matplot::plot(theta, yFit)->line_width(2).color("red");
  matplot::show();

  return std::pair<Eigen::VectorXd, Eigen::ArrayXd>(x, optIntensities);
};