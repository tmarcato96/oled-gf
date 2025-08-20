#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <fitting.hpp>
#include <matlayer.hpp>
#include <sweep.hpp>

#include <QApplication>
#include <QMainWindow>
#include <qvector.h>
#include <qwt_legend.h>
#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_zoomer.h>
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
  const std::filesystem::path fitFilePath = dataPath / "3ML_processed.txt";

  // Set up stack

  const double wavelength = 456;
  std::vector<Layer> layers;

  layers.emplace_back(Material(1.0, 0.0), -1.0);
  layers.emplace_back(Material(1.7, 0.0), 35e-9, true);
  layers.emplace_back(Material(1.52, 0.0), 5000e-9);
  layers.emplace_back(Material(1.52, 0.0), -1.0);

  // Spectrum
  // double fwhm = 30;
  // double sigma = fwhm / (2.0 * sqrt(2.0 * log(2.0)));
  // auto spectrum = std::make_shared<NormalDistribution>(450, 750, wavelength, sigma, 100);
  // auto spectrum = std::make_shared<FileDistribution>(spectrumFilePath.string());
  Distribution spectrum(wavelength);
  // Distribution dipolePos(0.0, 35e-9, 5);
  Distribution dipolePos(17.5e-9);

  auto solver = std::make_unique<Fitting>(fitFilePath, layers, 0.0, 90.0);
  SweepManager sm(*solver);
  sm.setSDSweep(dipolePos, spectrum);
  // sm.setSDSweep(dipolePos, std::static_pointer_cast<Distribution<>>(spectrum));
  //  sm.setSDSweep(dipolePos, spectrum);
  sm.runSweeps();
  Fitting::FitRes res = solver->fit();

  // for (size_t i = 0; i < fitRes.x.size(); ++i) {
  //   std::cout << fitRes.x[i] << " " << fitRes.yExp[i] << " " << fitRes.yFit[i] << "\n";
  // }

  QApplication app(argc, argv);
  QMainWindow window;

  QwtPlot* plot = new QwtPlot();

  QVector<double> x{res.x.begin(), res.x.end()};
  QVector<double> yExp{res.yExp.begin(), res.yExp.end()};
  QVector<double> yFit{res.yFit.begin(), res.yFit.end()};

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