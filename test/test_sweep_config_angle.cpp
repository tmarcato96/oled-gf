
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <indata.hpp>
#include <matlayer.hpp>
#include <simulation.hpp>
#include <sweep.hpp>

#include <QApplication>
#include <QMainWindow>
#include <qvector.h>
#include <qwt_legend.h>
#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_zoomer.h>
#include <qwt_scale_engine.h>
#include <qwt_symbol.h>

int main(int argc, char* argv[])
{
#ifndef PROJECT_ROOT
#error "PROJECT_ROOT is not defined. Define it via CMake with target_compile_definitions."
#endif

  // Fitting filepath
  const std::filesystem::path rootPath = PROJECT_ROOT;
  const std::filesystem::path dataPath = rootPath / "examples/data";
  const std::filesystem::path configFile = dataPath / "simulation_angle_test.json";

  auto importer = Data::ImportManager(configFile).makeImporter();
  auto solverManager = importer->solverFromFile();
  solverManager.sweepManager->runSweeps();
  if (auto* sim = dynamic_cast<Simulation*>(solverManager.solver.get())) { sim->calculateEmissionSubstrate(); }
  else {
    throw std::runtime_error("Couldn't downcast to fitting!");
  }

  // Plot
  QApplication app(argc, argv);
  QMainWindow window;

  QwtPlot* plot = new QwtPlot();

  Eigen::Index N = solverManager.solver->resultTree.get<Vector>("angle").size();
  QVector<double> theta(N), IPerp(N), IParaP(N), IParaS(N);
  Eigen::Map<Vector>(theta.data(), N) = solverManager.solver->resultTree.get<Vector>("angle");
  Eigen::Map<Vector>(IPerp.data(), N) = solverManager.solver->resultTree.get<Vector>("P_perp_sub");
  Eigen::Map<Vector>(IParaP.data(), N) = solverManager.solver->resultTree.get<Vector>("P_para_p_sub");
  Eigen::Map<Vector>(IParaS.data(), N) = solverManager.solver->resultTree.get<Vector>("P_para_s_sub");

  plot->setTitle("Simulation Results");
  plot->setCanvas(new QwtPlotCanvas());
  plot->setCanvasBackground(Qt::white);
  plot->setAxisTitle(QwtPlot::xBottom, "X");
  plot->setAxisTitle(QwtPlot::yLeft, "Y");
  plot->setAxisScale(QwtPlot::yLeft, 0.0, 1.0);

  QwtPlotCurve* IperpCurve = new QwtPlotCurve("Perp");
  IperpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
  IperpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
  IperpCurve->setPen(QPen(Qt::red));
  IperpCurve->setSamples(theta, IPerp);
  IperpCurve->attach(plot);

  QwtPlotCurve* IParaPCurve = new QwtPlotCurve("Para_p");
  IParaPCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
  IParaPCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
  IParaPCurve->setPen(QPen(Qt::blue));
  IParaPCurve->setSamples(theta, IParaP);
  IParaPCurve->attach(plot);

  QwtPlotCurve* IParaSCurve = new QwtPlotCurve("Para_s");
  IParaSCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
  IParaSCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
  IParaSCurve->setPen(QPen(Qt::green));
  IParaSCurve->setSamples(theta, IParaS);
  IParaSCurve->attach(plot);

  QwtPlotZoomer* zoomer = new QwtPlotZoomer(plot->canvas());
  zoomer->setRubberBandPen(QColor(Qt::red));
  zoomer->setTrackerPen(QColor(Qt::blue));
  QwtLegend* legend = new QwtLegend();
  plot->insertLegend(legend);

  window.setCentralWidget(plot);
  window.show();
  return app.exec();
}