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
  const std::filesystem::path configFile = dataPath / "simulation.json";

  auto manager = Data::ImportManager(configFile);
  auto importer = manager.makeImporter();
  auto solverManager = importer->solverFromFile();
  solverManager.sweepManager->runSweeps();
  auto simData = solverManager.sweepManager->getResults();

  QApplication app(argc, argv);
  QMainWindow window;

  QwtPlot* plot = new QwtPlot();

  QVector<double> u{simData.u.begin(), simData.u.end()};
  QVector<double> yParaUs{simData.yParaUsPol.begin(), simData.yParaUsPol.end()};
  QVector<double> yParaUp{simData.yParaUpPol.begin(), simData.yParaUpPol.end()};
  QVector<double> yPerp{simData.yPerp.begin(), simData.yPerp.end()};

  plot->setTitle("Simulation Results");
  plot->setCanvas(new QwtPlotCanvas());
  plot->setCanvasBackground(Qt::white);
  plot->setAxisTitle(QwtPlot::xBottom, "X");
  plot->setAxisTitle(QwtPlot::yLeft, "Y");
  plot->setAxisScaleEngine(QwtPlot::yLeft, new QwtLogScaleEngine());
  const double yMin = 1e-7;
  double yMax = std::max({*std::max_element(yParaUs.constBegin(), yParaUs.constEnd()),
    *std::max_element(yParaUp.constBegin(), yParaUp.constEnd()),
    *std::max_element(yPerp.constBegin(), yPerp.constEnd())});
  plot->setAxisScale(QwtPlot::yLeft, yMin, yMax);

  QwtPlotCurve* paraUsCurve = new QwtPlotCurve("s-Para");
  paraUsCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
  paraUsCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
  paraUsCurve->setPen(QPen(Qt::red));
  paraUsCurve->setSamples(u, yParaUs);
  paraUsCurve->attach(plot);

  QwtPlotCurve* paraUpCurve = new QwtPlotCurve("p-Para");
  paraUpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
  paraUpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
  paraUpCurve->setPen(QPen(Qt::blue));
  paraUpCurve->setSamples(u, yParaUp);
  paraUpCurve->attach(plot);

  QwtPlotCurve* perpCurve = new QwtPlotCurve("(p)-Perp");
  perpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
  perpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
  perpCurve->setPen(QPen(Qt::green));
  perpCurve->setSamples(u, yPerp);
  perpCurve->attach(plot);

  QwtPlotZoomer* zoomer = new QwtPlotZoomer(plot->canvas());
  zoomer->setRubberBandPen(QColor(Qt::red));
  zoomer->setTrackerPen(QColor(Qt::blue));
  QwtLegend* legend = new QwtLegend();
  plot->insertLegend(legend);

  window.setCentralWidget(plot);
  window.show();
  return app.exec();
}