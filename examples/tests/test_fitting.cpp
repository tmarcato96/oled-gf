#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <fitting.hpp>
#include <matlayer.hpp>

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
  // Set up stack
  double wavelength = 456;
  std::vector<Layer> layers;

  layers.emplace_back(Material(1.0, 0.0), -1.0);
  layers.emplace_back(Material(1.7, 0.0), 35e-9, true);
  layers.emplace_back(Material(1.52, 0.0), 5000e-9);
  layers.emplace_back(Material(1.52, 0.0), -1.0);

  // Fitting filepath
  // const std::string targetToFit("/src/examples/data/setfos_simple_spectrum_isotropic.txt");
  const std::string targetToFit("C:\\Users\\mnouman\\oled-gf\\examples\\data\\3ML_processed.txt");
  // Spectrum
  double fwhm = 30;
  NormalDistribution dist{450, 700, wavelength, fwhm / 2.355, 50};
  Spectrum<Distribution> spectrum{dist};
  Distribution dipoleDist(0.0, 35e-9);

  auto solver = std::make_unique<Fitting>(targetToFit, layers, 0.0, 456, 0.0, 80.0);
  auto fitRes = solver->fitEmissionSubstrate();
  // for (size_t i = 0; i < fitRes.x.size(); ++i) {
  //   std::cout << fitRes.x[i] << " " << fitRes.yExp[i] << " " << fitRes.yFit[i] << "\n";
  // }

  QApplication app(argc, argv);
  QMainWindow window;

  QwtPlot* plot = new QwtPlot();
  plot->setTitle("Fitting Results");
  plot->setCanvas(new QwtPlotCanvas());
  plot->setCanvasBackground(Qt::white);
  plot->setAxisTitle(QwtPlot::xBottom, "X");
  plot->setAxisTitle(QwtPlot::yLeft, "Y");

  QVector<double> xData(fitRes.x.begin(), fitRes.x.end());
  QVector<double> yExpData(fitRes.yExp.begin(), fitRes.yExp.end());
  QVector<double> yFitData(fitRes.yFit.begin(), fitRes.yFit.end());

  QwtPlotCurve* scatterCurve = new QwtPlotCurve("Exp");
  QwtSymbol* symbol = new QwtSymbol(QwtSymbol::Triangle, QBrush(Qt::blue), QPen(Qt::black), QSize(8, 8));
  scatterCurve->setSymbol(symbol);
  scatterCurve->setStyle(QwtPlotCurve::NoCurve); // No connecting line
  scatterCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, true);
  scatterCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, false);
  scatterCurve->setSamples(xData, yExpData);
  scatterCurve->attach(plot);

  QwtPlotCurve* fitCurve = new QwtPlotCurve("Fit");
  fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
  fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
  fitCurve->setSamples(xData, yFitData);
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