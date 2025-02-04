#include <configuration.hpp>

#include <iostream>
#include <matplot/matplot.h>

int main(int argc, char* argv[]) {
    //std::string configFile("/src/examples/tests/simple_emitter_with_spectrum_and_dipole_dist.json");
    std::string configFile(argv[1]);
    SimulationManager manager(configFile);
    auto solver = manager.create();

    solver->run();


    // Polar figure
    Eigen::ArrayXd thetaGlass, powerPerpAngleGlass, powerParasPolAngleGlass, powerParapPolAngleGlass;
    solver->calculateEmissionSubstrate(thetaGlass, powerPerpAngleGlass, powerParapPolAngleGlass, powerParasPolAngleGlass);

    matplot::figure();
    matplot::plot(thetaGlass, powerPerpAngleGlass, "-o");
    matplot::hold(matplot::on);
    matplot::plot(thetaGlass, powerParapPolAngleGlass, "-o");
    matplot::plot(thetaGlass, powerParasPolAngleGlass, "-o");
    
    matplot::figure();
    matplot::polarplot(thetaGlass, powerPerpAngleGlass, "-")->line_width(2);
    matplot::hold(matplot::on);
    matplot::polarplot(thetaGlass, powerParapPolAngleGlass, "-")->line_width(2);
    matplot::polarplot(thetaGlass, powerParasPolAngleGlass, "-")->line_width(2);

    matplot::show();
}