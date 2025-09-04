
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
  const std::filesystem::path configFile = dataPath / "fitting.json";

  auto importer = Data::ImportManager(configFile).makeImporter();
  auto solverManager = importer->solverFromFile();
  solverManager.sweepManager->runSweeps();
  if (auto* fit = dynamic_cast<Fitting*>(solverManager.solver.get())) { fit->fit(); }
  else {
    throw std::runtime_error("Couldn't downcast to fitting!");
  }

  // Plot
  QApplication app(argc, argv);
  QMainWindow window;

  QwtPlot* plot = new QwtPlot();

  Eigen::Index N = solverManager.solver->resultTree.get<Vector>("angle_exp").size();
  QVector<double> x(N), yExp(N), yFit(N);
  Eigen::Map<Vector>(x.data(), N) = solverManager.solver->resultTree.get<Vector>("angle_exp");
  Eigen::Map<Vector>(yExp.data(), N) = solverManager.solver->resultTree.get<Vector>("I_angle_exp");
  Eigen::Map<Vector>(yFit.data(), N) = solverManager.solver->resultTree.get<Vector>("I_angle_fit");

  plot->setTitle("Simulation Results");
  plot->setCanvas(new QwtPlotCanvas());
  plot->setCanvasBackground(Qt::white);
  plot->setAxisTitle(QwtPlot::xBottom, "X");
  plot->setAxisTitle(QwtPlot::yLeft, "Y");

  QwtPlotCurve* expCurve = new QwtPlotCurve("Exp");
  expCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
  expCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
  expCurve->setPen(QPen(Qt::red));
  expCurve->setSamples(x, yExp);
  expCurve->attach(plot);

  QwtPlotCurve* fitCurve = new QwtPlotCurve("Fit");
  fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
  fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
  fitCurve->setPen(QPen(Qt::blue));
  fitCurve->setSamples(x, yFit);
  fitCurve->attach(plot);

  QwtPlotZoomer* zoomer = new QwtPlotZoomer(plot->canvas());
  zoomer->setRubberBandPen(QColor(Qt::red));
  zoomer->setTrackerPen(QColor(Qt::blue));
  QwtLegend* legend = new QwtLegend();
  plot->insertLegend(legend);

  window.setCentralWidget(plot);
  window.show();
  return app.exec();
}