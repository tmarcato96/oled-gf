#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <Eigen/Core>
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
  const std::filesystem::path matPath = rootPath / "mat";
  const std::filesystem::path dataPath = rootPath / "examples/data";
  const std::filesystem::path spectrumFilePath = dataPath / "6ML_PL.txt";

  // Set up stack

  std::vector<Layer> layers;

  layers.emplace_back(Material(matPath / "Al_Cent.csv", ','), -1.0);
  layers.emplace_back(Material(1.9, 0.0), 50e-9);
  layers.emplace_back(Material(matPath / "CBP.csv", ','), 20e-9, true);
  layers.emplace_back(Material(matPath / "PEDOT_BaytronP_AL4083.csv", ','), 35e-9);
  layers.emplace_back(Material(matPath / "ITO.csv", ','), 150e-9);
  layers.emplace_back(Material(1.52, 0.0), 5000e-9);
  layers.emplace_back(Material(1.52, 0.0), -1.0);

  // Spectrum
  // auto spectrum = std::make_shared<FileDistribution>(spectrumFilePath.string());
  double fwhm = 30;
  double sigma = fwhm / (2.0 * sqrt(2.0 * log(2.0)));
  auto spectrum = std::make_shared<NormalDistribution>(450, 750, 550, sigma, 40);
  // Distribution spectrum(550);
  Distribution dipolePos(0.0, 20e-9, 5);
  // Distribution dipolePos(10e-9);

  const double uStart = 0.0;
  const double uStop = 2.0;
  auto solver = std::make_unique<Simulation>(SimulationMode::ModeDissipation, layers, uStart, uStop);
  SweepManager sm(*solver);
  sm.setSDSweep(dipolePos, std::static_pointer_cast<Distribution<>>(spectrum));
  // sm.setSDSweep(dipolePos, spectrum);
  sm.runSweeps();
  SweepManager::SimRes simData = sm.getResults();

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
  plot->setAxisScale(QwtPlot::xBottom, uStart, uStop);

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