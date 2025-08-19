#include <algorithm>
#include <filesystem>
#include <fstream>

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

  // Material filepath
  const std::filesystem::path rootPath = PROJECT_ROOT;
  const std::filesystem::path matPath = rootPath / "mat";

  // Set up stack
  std::vector<Layer> layers;

  layers.emplace_back(Material(1.0, 0.0), -1.0);
  layers.emplace_back(Material(1.578, 0.0), 141e-9, true);
  layers.emplace_back(Material(0.0715, 4.1958), 5000e-9);
  layers.emplace_back(Material(0.0715, 4.1958), -1.0);

  // Spectrum
  const double wavelength = 550;
  Distribution spectrum(wavelength);

  // Dipole position
  Distribution dipolePos(0.0);

  // Run
  auto solver = std::make_unique<Simulation>(SimulationMode::ModeDissipation, layers, 0.0, 2.0);
  SweepManager sm(*solver);
  sm.setSDSweep(dipolePos, spectrum);
  sm.runSweeps();

  SweepManager::SimRes simData = sm.getResults();

  // Temp plot
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
