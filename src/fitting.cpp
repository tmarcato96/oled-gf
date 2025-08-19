#define _USE_MATH_DEFINES
#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <unsupported/Eigen/NonLinearOptimization>
#include <vector>

#include "matlayer.hpp"
#include <fitting.hpp>

Fitting::Fitting(const std::string& fittingFilePath,
  const std::vector<Layer>& layers,
  const double sweepStart,
  const double sweepStop) :
  BaseSolver(layers, sweepStart, sweepStop)
{
  intensityData = Data::loadFromFile(fittingFilePath, 2);
}

Fitting::Fitting(const Matrix& fitData,
  const std::vector<Layer>& layers,
  const double sweepStart,
  const double sweepStop) :
  BaseSolver(layers, sweepStart, sweepStop),
  intensityData{fitData}
{}

void Fitting::update()
{
  // Log initialization of Simulation
  std::cout << "\n\n\n"
            << "-----------------------------------------------------------------\n";
  std::cout << "              Initializing Fitting             \n";
  std::cout << "-----------------------------------------------------------------\n"
            << "\n\n";

  this->discretize();
}

void Fitting::setup()
{
  // setting up functor for fitting
  residual.intensities = intensityData.col(1).segment(0, matstack.u.size());
  calculateEmissionSubstrate();
  Matrix powerGlassP(2, resultTree.get<Vector>("P_perp_sub").size());
  powerGlassP.row(0) = resultTree.get<Vector>("P_perp_sub");
  powerGlassP.row(1) = resultTree.get<Vector>("P_para_p_sub");
  residual.powerGlass = std::move(powerGlassP);
}

void Fitting::genInPlaneWavevector()
{
  // Technically should not be updated every run of the sweep
  // Cumulative sum of thicknesses
  matstack.z0.resize(matstack.numLayers - 1);
  matstack.z0(0) = 0.0;
  std::vector<double> thicknesses;
  for (size_t i = 1; i < layers.size() - 1; ++i) { thicknesses.push_back(layers[i].getThickness()); }
  std::partial_sum(thicknesses.begin(), thicknesses.end(), std::next(matstack.z0.begin()), std::plus<double>());
  matstack.z0 -= (matstack.z0(dipoleLayer - 1) + dipolePosition);

  // getting sim data
  Vector u_range = Eigen::real(Eigen::sqrt(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer) *
                                           (1 - pow(Eigen::cos(intensityData.col(0)), 2))));
  matstack.u = u_range.head(u_range.size() - 1);
  matstack.x = matstack.u.acos();
  matstack.numKVectors = matstack.u.size();

  // Differences
  matstack.dU = u_range.segment(1, u_range.size() - 1) - u_range.segment(0, u_range.size() - 1);
  matstack.dX = u_range.segment(1, u_range.size() - 1).acos() - u_range.segment(0, u_range.size() - 1).acos();
}

void Fitting::genOutofPlaneWavevector()
{
  // Out of plane wavevector
  matstack.k = 2 * M_PI / wvl / 1e-9 * matstack.epsilon.sqrt();
  matstack.h.resize(matstack.numLayers, matstack.u.size());
  matstack.h = matstack.k(dipoleLayer) *
               (((matstack.epsilon.replicate(1, matstack.x.size())) / matstack.epsilon(dipoleLayer)).rowwise() -
                 matstack.u.pow(2).transpose())
                 .sqrt();
}

void Fitting::discretize()
{
  loadMaterialData();
  genInPlaneWavevector();
  genOutofPlaneWavevector();
}

int ResFunctor::operator()(const Eigen::VectorXd& params, Eigen::VectorXd& fvec) const
{
  // x here is vector of fitting params
  for (size_t i = 0; i < intensities.size(); ++i) {
    fvec(i) = intensities(i) - params(0) * (params(1) * powerGlass(0, i) +
                                             (1 - params(1)) * powerGlass(1, i)); // residual of each sample
  }
  return 0;
}

int ResFunctor::inputs() const { return 2; }

int ResFunctor::values() const { return intensities.size(); }

Fitting::FitRes Fitting::fit()
{
  setup();

  // returns the vector of parameters and the fitted intensities as a std::pair
  std::vector<double> theta(matstack.x.rows()), yFit(matstack.x.rows()), yExp(residual.intensities.rows());

  Eigen::ArrayXd::Map(&theta[0], intensityData.rows() - 1) = intensityData.col(0).segment(0, matstack.u.size());
  Eigen::ArrayXd::Map(&yExp[0], intensityData.rows() - 1) = residual.intensities;

  // Setup
  Eigen::VectorXd fitParams(2);
  // Initial guess
  fitParams(0) = 1.0;
  fitParams(1) = 0.34;

  Eigen::LevenbergMarquardt<ResFunctorNumericalDiff> lm(residual);
  lm.parameters.maxfev = 2000;
  lm.parameters.xtol = 1e-8;
  lm.parameters.ftol = 1e-8;

  int status = lm.minimize(fitParams);
  std::cout << "Number of iterations: " << lm.iter << '\n';
  std::cout << "Status: " << status << '\n';
  std::cout << "Fitting result: " << fitParams << '\n' << '\n';

  // simulation results
  alpha = fitParams(1);
  Eigen::ArrayXd optIntensities(matstack.x.rows());
  optIntensities =
    fitParams(0) * (fitParams(1) * residual.powerGlass.row(0) + (1 - fitParams(1)) * residual.powerGlass.row(1));
  Eigen::ArrayXd::Map(&yFit[0], matstack.x.rows()) = optIntensities;

  Fitting::FitRes res{yExp, yFit, theta, fitParams};

  return res;
};

void Fitting::calculateEmissionSubstrate()
{
  CMatrix& powerPerpUpPol = resultTree.get<CMatrix>("P_perp_u");
  CMatrix& powerParaUpPol = resultTree.get<CMatrix>("P_para_p_u");

  resultTree.insertAs<Vector>(Vector(), POWER_DIPOLES_SUB);
  Eigen::Index substrateIndex = matstack.numLayers - 2; // glass

  resultTree.get<Vector>("P_perp_sub") =
    powerPerpUpPol.row(substrateIndex).real() *
    std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer)));
  resultTree.get<Vector>("P_perp_sub") /= Eigen::tan(intensityData.col(0).segment(0, matstack.u.size()));

  resultTree.get<Vector>("P_para_p_sub") =
    powerParaUpPol.row(substrateIndex).real() *
    std::sqrt(std::real(matstack.epsilon(matstack.numLayers - 1) / matstack.epsilon(dipoleLayer)));
  resultTree.get<Vector>("P_para_p_sub") /= Eigen::tan(intensityData.col(0).segment(0, matstack.u.size()));
}