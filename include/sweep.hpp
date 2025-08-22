#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include <basesolver.hpp>
#include <distribution.hpp>
#include <simulation.hpp>
#include <utils.hpp>

template<typename StepFn>
void integrateTrapezoidal(Eigen::Index N, const Vector& X, StepFn&& step, double totInterval = 1.0)
{
  if (N <= 0 || X.size() < N) throw std::runtime_error("integrateTrapezoidal: invalid sizes");
  if (N < 2) return;

  BaseSolver* solver = step(0);

  // Result matrices
  using resMap = decltype(solver->resultTree);
  resMap out;
  out.initializeFromWithZeros(solver->resultTree, POWER_DIPOLES);

  resMap prev;
  prev = solver->resultTree(POWER_DIPOLES);

  for (Eigen::Index i = 1; i < N; ++i) {
    const double dX = (X(i) - X(i - 1)) / totInterval;
    step(i); // mutates solver->resultTree
    out(POWER_DIPOLES) += (solver->resultTree(POWER_DIPOLES) + prev(POWER_DIPOLES)) * 0.5 * dX;

    prev(POWER_DIPOLES) = solver->resultTree(POWER_DIPOLES);
  }

  // Put results in solver
  solver->resultTree(POWER_DIPOLES).moveFrom(out(POWER_DIPOLES));
}

class ISweep
{
public:
  virtual void update() = 0;
  virtual ~ISweep() = default;
};

template<typename DipoleT, typename SpectrumT> class SweepSpecDip : public ISweep
{

  void runSingleValue(double dipolePos, double wavelength, bool reuseDipole = false)
  {
    if (!reuseDipole) solver->setDipolePosition(dipolePos);

    solver->setWavelength(wavelength);
    // Calculate
    solver->update();
    solver->run();
  }

  void runSingleDistribution()
  {
    if constexpr (std::is_same_v<SpectrumT, Matrix>) {
      const Eigen::Index N = spectrum.rows();
      integrateTrapezoidal(N, spectrum.col(0), [&](Eigen::Index i) {
        // Make sure dipolePositions is right type
        double dipolePos = [&] {
          if constexpr (std::is_same_v<DipoleT, Matrix>) return dipolePositions(0, 0);
          else return dipolePositions;
        }();

        runSingleValue(dipolePos, spectrum(i, 0), true);
        double weight = spectrum(i, 1);
        solver->resultTree(POWER_DIPOLES) *= weight;
        return solver;
      });
    }
    else if constexpr (std::is_same_v<DipoleT, Matrix>) {
      const Eigen::Index N = dipolePositions.rows();
      integrateTrapezoidal(
        N,
        dipolePositions.col(0),
        [&](Eigen::Index i) {
          runSingleValue(dipolePositions(i, 0), spectrum);
          return solver;
        },
        solver->getLayerThickness(solver->getDipoleIndex()));
    }
    else {
      runSingleValue(dipolePositions, spectrum);
    }
  }

public:
  SweepSpecDip(BaseSolver* solver, Distribution<DipoleT> dipoleDist, Distribution<SpectrumT> spectrumDist) :
    solver{solver},
    dipolePositions{std::move(dipoleDist.values)},
    spectrum{std::move(spectrumDist.values)}
  {}

  void update() override
  {
    if constexpr (std::is_same_v<DipoleT, Matrix> && std::is_same_v<SpectrumT, Matrix>) {
      // dual distribution
      const Eigen::Index N = dipolePositions.rows();
      integrateTrapezoidal(
        N,
        dipolePositions.col(0),
        [&](Eigen::Index i) {
          solver->setDipolePosition(dipolePositions(i, 0));
          // inner spectral integration
          const Eigen::Index M = spectrum.rows();
          integrateTrapezoidal(M, spectrum.col(0), [&](Eigen::Index j) {
            runSingleValue(dipolePositions(i, 0), spectrum(j, 0), true);
            double w = spectrum(j, 1);
            solver->resultTree(POWER_DIPOLES) *= w;
            return solver;
          });
          return solver;
        },
        solver->getLayerThickness(solver->getDipoleIndex()));
    }
    else {
      runSingleDistribution();
    }
  }

private:
  BaseSolver* solver;

public:
  DipoleT dipolePositions;
  SpectrumT spectrum;
};

class SweepLayer : public ISweep
{
  std::map<size_t, double> _sweepParams;

public:
  SweepLayer(size_t layerNum, double thickness) { _sweepParams.emplace(layerNum, thickness); }
  explicit SweepLayer(const std::map<size_t, double>& sweepParams) :
    _sweepParams{sweepParams}
  {}

  void update() override {}

  bool isEmpty() const { return _sweepParams.empty(); }
};

class SweepManager
{
public:
  explicit SweepManager(BaseSolver& solver) :
    _solver{&solver}
  {}

  template<class DipArg, class SpecArg> void setSDSweep(DipArg&& dipArg, SpecArg&& specArg)
  {
    using DipT = dist::deduced_value_t<DipArg>;
    using SpecT = dist::deduced_value_t<SpecArg>;

    static_assert(std::is_same_v<DipT, double> || std::is_same_v<DipT, Matrix>, "Dipole must be double or Matrix");
    static_assert(std::is_same_v<SpecT, double> || std::is_same_v<SpecT, Matrix>, "Spectrum must be double or Matrix");

    auto dipole = dist::as_distribution(std::forward<DipArg>(dipArg));
    auto spectrum = dist::as_distribution(std::forward<SpecArg>(specArg));

    _sdSweep = std::make_unique<SweepSpecDip<DipT, SpecT>>(_solver, std::move(dipole), std::move(spectrum));
  }

  void addSweep(size_t layerNum, Distribution<> thicknesses) { _sweeps.emplace(layerNum, thicknesses); }

  void runSweeps()
  {
    if (!_solver) throw std::runtime_error("SweepManager: no solver attached.");

    if (isEmpty()) {
      if (_sdSweep) { _sdSweep->update(); }
      else {
        throw std::runtime_error("Cannot run sweep without Dipole and Spectrum!");
      }
    }
    else {
      // Generate SweepTable
      fillSweepTable();
    }
  }

  struct SimRes
  { // this thing only exists to make plotting easier just like FitRes. (also needs testing)
    std::vector<double> u;
    std::vector<double> yPerp, yParaUpPol, yParaUsPol;
  };

  SimRes getResults()
  {
    const Eigen::Index N = _solver->resultTree.get<Vector>("u").rows();
    auto dipoleLayer = _solver->getDipoleIndex() - 1;

    std::vector<double> u(toSize(N)), powerPerp(toSize(N)), powerParaUs(toSize(N)), powerParaUp(toSize(N));

    // Use Eigen::Map to copy Eigen arrays into std::vector
    Eigen::Map<Eigen::ArrayXd>(powerPerp.data(), N) =
      _solver->resultTree.get<Matrix>("P_perp_uf").row(toIndex(dipoleLayer));
    Eigen::Map<Eigen::ArrayXd>(powerParaUs.data(), N) =
      _solver->resultTree.get<Matrix>("P_para_p_uf").row(toIndex(dipoleLayer));
    Eigen::Map<Eigen::ArrayXd>(powerParaUp.data(), N) =
      _solver->resultTree.get<Matrix>("P_para_s_uf").row(toIndex(dipoleLayer));
    Eigen::Map<Eigen::ArrayXd>(u.data(), N) = _solver->resultTree.get<Vector>("u");

    return SimRes{u, powerPerp, powerParaUs, powerParaUp};
  }

private:
  using sweep = std::map<size_t, double>; // current combination of sweepParams
  using sweepsIt = std::map<size_t, Distribution<>>::const_iterator;
  using sweepIt = sweep::const_iterator;
  void generateSweepTable(sweep& combination, sweepsIt selector, sweepsIt end)
  {

    if (selector == end) {
      _sweepTable.push_back(SweepLayer(combination));
      return;
    }

    const Matrix& tMatrix = selector->second.values;
    const size_t layerNum = selector->first;
    for (auto d : tMatrix.col(0)) {
      combination[layerNum] = d;
      generateSweepTable(combination, std::next(selector), end);
      combination.erase(layerNum);
    }
  }

  void fillSweepTable()
  {
    sweep temp;
    generateSweepTable(temp, _sweeps.begin(), _sweeps.end());
  }

  bool isEmpty() const { return _sweeps.empty(); }

  BaseSolver* _solver; // non-owning
  std::unique_ptr<ISweep> _sdSweep;
  std::map<size_t, Distribution<>> _sweeps;
  std::vector<SweepLayer> _sweepTable;
};

struct SolverManager
{
  using SlvrPtr = std::unique_ptr<BaseSolver>;
  using SmPtr = std::unique_ptr<SweepManager>;

  SlvrPtr solver;
  SmPtr sweepManager;

  SolverManager(SlvrPtr s, SmPtr sm) :
    solver{std::move(s)},
    sweepManager{std::move(sm)}
  {}
};
