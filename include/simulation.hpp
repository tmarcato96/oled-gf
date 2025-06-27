/*! \file simulation.hpp
    \brief A header file for energy emission simulation.

    The simulation header is used to define the simulation class
    and auxiliar functionalities which are used to simulate the
    energy emission behavior of the stack in question.
*/
#pragma once

#include <Eigen/Core>
#include <string>
#include <vector>

#include <basesolver.hpp>
#include <matlayer.hpp>

/*! \class Simulation
    \brief A class for energy emission simulation which inherits from baseSolver.

    The Simulation class is used to perform energy emission simulations for the material
    stack of interest. It inherits from the baseSolver base class which acts as an interface
    for both the Simulation and Fitting classes. Thus, it shares its basic methods and members
    with the Fitting class and includes additional ones especially intented for emissions
    simulation.
*/

enum class SimulationMode { AngleSweep, ModeDissipation };

class Simulation : public BaseSolver
{
protected:
  void genInPlaneWavevector() override;
  void genOutofPlaneWavevector() override;
  void discretize() override;

  void init();

  SimulationMode _mode;

public:
  struct SimRes
  { // this thing only exists to make plotting easier just like FitRes. (also needs testing)
    std::vector<double> u;
    std::vector<double> yPerp, yParaUpPol, yParaUsPol;
  };

  SimRes powerModeDissipation();

  Simulation(SimulationMode mode,
    const std::vector<Layer>& layers,
    const double dipolePosition,
    Spectrum<Distribution> spectrum,
    const double sweepStart,
    const double sweepStop,
    const double alpha = 1.0 / 3.0);

  Simulation(SimulationMode mode,
    const std::vector<Layer>& layers,
    const Distribution& dipoleDist,
    Spectrum<Distribution> spectrum,
    const double sweepStart,
    const double sweepStop,
    const double alpha = 1.0 / 3.0);

  ~Simulation() = default;
};
