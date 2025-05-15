#define _USE_MATH_DEFINES
#include <Eigen/Core>
#include <unsupported/Eigen/NonLinearOptimization>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>
#include <matplot/matplot.h>

#include "indata.hpp"
#include "fitting.hpp"
#include "linalg.hpp"
#include "matlayer.hpp"
#include "basesolver.hpp"


Fitting::Fitting(const std::string& fittingFilePath,
                 const std::vector<Layer>& layers,
                 const double dipolePosition,
                 const double wavelength,
                 const double sweepStart,
                 const double sweepStop):
                 BaseSolver(layers,
                 dipolePosition,
                 wavelength,
                 sweepStart,
                 sweepStop) {
                 init(std::move(fittingFilePath));
                }

Fitting::Fitting(Matrix& fitData,
                const std::vector<Layer>& layers,
                const double dipolePosition,
                const double wavelength,
                const double sweepStart,
                const double sweepStop) :
                BaseSolver(layers,
                            dipolePosition,
                            wavelength,
                            sweepStart,
                            sweepStop),
                intensityData{fitData} {
                  std::string empty{};
                  init(empty);
                }

Fitting::Fitting(const std::string& fittingFilePath,
                 const std::vector<Layer>& layers,
                 const double dipolePosition,
                 const std::string& spectrumFile,
                 const double sweepStart,
                 const double sweepStop):
                 BaseSolver(layers,
                            dipolePosition,
                            spectrumFile,
                            sweepStart,
                            sweepStop) {
                 init(std::move(fittingFilePath));
                }

Fitting::Fitting(const std::string& fittingFilePath,
                 const std::vector<Layer>& layers,
                 const double dipolePosition,
                 const GaussianSpectrum& spectrum,
                 const double sweepStart,
                 const double sweepStop):
                 BaseSolver(layers,
                            dipolePosition,
                            spectrum,
                            sweepStart,
                            sweepStop) {
                 init(std::move(fittingFilePath));
                }

Fitting::Fitting(const Matrix& fitData,
                const std::vector<Layer>& layers,
                const double dipolePosition,
                const GaussianSpectrum& spectrum,
                const double sweepStart,
                const double sweepStop) :
                BaseSolver(layers,
                dipolePosition,
                spectrum,
                sweepStart,
                sweepStop),
                intensityData{fitData} {
                std::string empty{};
                init(empty);
              }

Fitting::Fitting(const std::string& fittingFilePath,
                 const std::vector<Layer>& layers,
                 const DipoleDistribution& dipoleDist,
                 const double wavelength,
                 const double sweepStart,
                 const double sweepStop):
                 BaseSolver(layers,
                            dipoleDist,
                            wavelength,
                            sweepStart,
                            sweepStop) {
                 init(std::move(fittingFilePath));
                }

Fitting::Fitting(const Matrix& fitData,
                 const std::vector<Layer>& layers,
                 const DipoleDistribution& dipoleDist,
                 const double wavelength,
                 const double sweepStart,
                 const double sweepStop):
                 BaseSolver(layers,
                            dipoleDist,
                            wavelength,
                            sweepStart,
                            sweepStop),
                            intensityData{fitData} {
                std::string empty{};
                init(empty);
              }

Fitting::Fitting(const std::string& fittingFilePath,
                 const std::vector<Layer>& layers,
                 const DipoleDistribution& dipoleDist,
                 const GaussianSpectrum& spectrum,
                 const double sweepStart,
                 const double sweepStop):
                 BaseSolver(layers,
                            dipoleDist,
                            spectrum,
                            sweepStart,
                            sweepStop) {
                 init(std::move(fittingFilePath));
                }

Fitting::Fitting(const Matrix& fitData,
                 const std::vector<Layer>& layers,
                 const DipoleDistribution& dipoleDist,
                 const GaussianSpectrum& spectrum,
                 const double sweepStart,
                 const double sweepStop) :
                 BaseSolver(layers,
                  dipoleDist,
                  spectrum,
                  sweepStart,
                  sweepStop),
                 intensityData{fitData} {
                 std::string empty{};
                 init(empty);
                }

void Fitting::init(const std::string& fittingFile) {
  // Log initialization of Simulation
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Initializing Fitting             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";

  if (!fittingFile.empty()) intensityData = Data::loadFromFile(fittingFile, 2);
  else if (intensityData.size() == 0) throw std::runtime_error("fitting data improperly initialized!");
  this->discretize();
  run();
  //setting up functor for fitting
  residual.intensities = intensityData.col(1).segment(0, matstack.u.size());
  residual.powerGlass = calculateEmissionSubstrate();
}

void Fitting::genInPlaneWavevector() {
  // Cumulative sum of thicknesses
  matstack.z0.resize(matstack.numLayers - 1);
  matstack.z0(0) = 0.0;
  std::vector<double> thicknesses;
  for (size_t i=1; i < layers.size()-1; ++i) {
    thicknesses.push_back(layers[i].getThickness());
  }
  std::partial_sum(thicknesses.begin(), thicknesses.end(), std::next(matstack.z0.begin()), std::plus<double>());
  matstack.z0 -= (matstack.z0(dipoleLayer - 1) + dipolePosition);

//getting sim data
  Vector u_range = Eigen::real(Eigen::sqrt(matstack.epsilon(matstack.numLayers - 1)/matstack.epsilon(dipoleLayer)*(1- pow(Eigen::cos(intensityData.col(0)), 2))));
  matstack.u = u_range.head(u_range.size() - 1);
  matstack.x = matstack.u.acos();
  matstack.numKVectors = matstack.u.size();
  
  // Differences
  matstack.dU = u_range.segment(1, u_range.size()-1) - u_range.segment(0, u_range.size()-1);
  matstack.dX = u_range.segment(1, u_range.size()-1).acos() - u_range.segment(0, u_range.size()-1).acos();
}

void Fitting::genOutofPlaneWavevector() {
  // Out of plane wavevector
  matstack.k = 2 * M_PI / wvl / 1e-9 * matstack.epsilon.sqrt();
  matstack.h.resize(matstack.numLayers, matstack.u.size());
  matstack.h = matstack.k(dipoleLayer) *
               (((matstack.epsilon.replicate(1, matstack.x.size())) / matstack.epsilon(dipoleLayer)).rowwise() -
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
  Eigen::Index substrateIndex = matstack.numLayers-2; //glass

  powerPerppPolGlass = powerPerpUpPol.row(substrateIndex).real() *
                    std::sqrt(std::real(matstack.epsilon(matstack.numLayers-1) / matstack.epsilon(dipoleLayer)));
  powerPerppPolGlass /= Eigen::tan(intensityData.col(0).segment(0, matstack.u.size()));

  powerParapPolGlass = powerParaUpPol.row(substrateIndex).real() *
                    std::sqrt(std::real(matstack.epsilon(matstack.numLayers-1) / matstack.epsilon(dipoleLayer)));
  powerParapPolGlass /= Eigen::tan(intensityData.col(0).segment(0, matstack.u.size()));


  Matrix powerGlass(2, powerPerppPolGlass.size());
  powerGlass.row(0) = powerPerppPolGlass;
  powerGlass.row(1) = powerParapPolGlass;
  return powerGlass;
}

int ResFunctor::operator()(const Eigen::VectorXd& params, Eigen::VectorXd& fvec) const {
  // x here is vector of fitting params 
  for (size_t i = 0; i < intensities.size(); ++i) {
    fvec(i) = intensities(i) - params(0) * (params(1)*powerGlass(0, i) + (1 - params(1))*powerGlass(1, i)); // residual of each sample
  }
  return 0;
} 

int ResFunctor::inputs() const {return 2;}

int ResFunctor::values() const {return intensities.size();}

std::pair<Eigen::VectorXd, Eigen::ArrayXd> Fitting::fitEmissionSubstrate() {
  //returns the vector of parameters and the fitted intensities as a std::pair

  std::vector<double> theta(matstack.x.rows()), yFit(matstack.x.rows()), yExp(residual.intensities.rows());


  Eigen::ArrayXd::Map(&theta[0], intensityData.rows()-1) = intensityData.col(0).segment(0, matstack.u.size());
  Eigen::ArrayXd::Map(&yExp[0], intensityData.rows()-1) = residual.intensities;

  // Setup
  Eigen::VectorXd fitParams(2);
  // Initial guess
  fitParams(0) = 1.0;
  fitParams(1) = 0.19;

  Eigen::LevenbergMarquardt<ResFunctorNumericalDiff> lm(residual);
  lm.parameters.maxfev = 2000;
  lm.parameters.xtol = 1e-10;
  lm.parameters.ftol = 1e-10;

  int status = lm.minimize(fitParams);
  std::cout << "Number of iterations: " << lm.iter << '\n';
  std::cout << "Status: " << status << '\n';
  std::cout << "Fitting result: " << fitParams << '\n' << '\n';

  // simulation results
  alpha = fitParams(1);
  Eigen::ArrayXd optIntensities(matstack.x.rows());
  optIntensities = fitParams(0) * (fitParams(1)*residual.powerGlass.row(0) + (1 - fitParams(1))*residual.powerGlass.row(1));
  Eigen::ArrayXd::Map(&yFit[0], matstack.x.rows()) = optIntensities;

  //plotting the results
  matplot::figure();
  
  matplot::scatter(theta, yExp);
  matplot::hold(matplot::on);
  matplot::plot(theta, yFit)->line_width(2).color("red");
  matplot::show();

  return std::pair<Eigen::VectorXd, Eigen::ArrayXd>(fitParams, optIntensities);
};