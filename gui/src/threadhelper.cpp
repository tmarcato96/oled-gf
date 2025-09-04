#define _USE_MATH_DEFINES
#include "threadhelper.h"

#include <fstream>
#include <set>
#include <string>

#include <Eigen/Core>
#include <basesolver.hpp>
#include <fitting.hpp>
#include <indata.hpp>
#include <polymap.hpp>
#include <simulation.hpp>

#include <QMutex>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <QMainWindow>
#include <QPen>
#include <QScrollArea>
#include <QwtPlot>
#include <QwtPlotZoomer>
#include <qwt_legend.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_zoomer.h>
#include <qwt_symbol.h>

#include <qwt_point_polar.h>
#include <qwt_polar_canvas.h>
#include <qwt_polar_curve.h>
#include <qwt_polar_grid.h>
#include <qwt_polar_magnifier.h>
#include <qwt_polar_marker.h>
#include <qwt_polar_renderer.h>
#include <qwt_scale_engine.h>
#include <qwt_series_data.h>

using namespace UIthreading;

struct PolarData : QwtSeriesData<QwtPointPolar>
{
  QVector<QwtPointPolar> pts;
  size_t size() const override { return static_cast<size_t>(pts.size()); }
  QwtPointPolar sample(size_t i) const override { return pts[static_cast<int>(i)]; }
  QRectF boundingRect() const override
  {
    static QRectF rect;
    if (rect.width() < 0.0) rect = qwtBoundingRect(*this);
    return rect;
  }
};

std::set<QString> Worker::_blacklist{};

Worker::Worker(const QString& filepath) :
  _filepath{filepath}
{
  emit solverStatus(0);
}

void Worker::startSolver()
{
  _workerMutex.lock();
  emit solverStatus(0);
  if (_blacklist.find(_filepath) != _blacklist.end()) {
    emit errorSignal("Solver already exists!");
    return;
  }
  _blacklist.insert(_filepath);
  auto importer = Data::ImportManager(_filepath.toStdString()).makeImporter();
  _solver = importer->solverFromFile();
  _solver.sweepManager->runSweeps(); // heavier computations

  if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
  else {
    _mode = Data::SolverMode::simulation;
  }

  _workerMutex.unlock();
  emit solverStatus(1);
}

void Worker::restartSolver()
{
  _workerMutex.lock();
  emit solverStatus(0);
  if (_blacklist.find(_filepath) == _blacklist.end()) {
    emit errorSignal("Start the solver first before attempting to restart!");
    return;
  }
  auto importer = Data::ImportManager(_filepath.toStdString()).makeImporter();
  _solver = importer->solverFromFile(); // heavier computations
  _solver.sweepManager->runSweeps();

  if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
  else {
    _mode = Data::SolverMode::simulation;
  }

  _workerMutex.unlock();
  emit solverStatus(1);
}

void Worker::restartSolver(const QString& solverPath)
{
  _workerMutex.lock();
  if (_blacklist.find(_filepath) == _blacklist.end()) {
    emit errorSignal("Start the solver first before attempting to restart!");
    return;
  }
  emit solverStatus(0);
  _filepath = solverPath;

  auto importer = Data::ImportManager(_filepath.toStdString()).makeImporter();
  _solver = importer->solverFromFile();
  _solver.sweepManager->runSweeps(); // heavier computations

  if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
  else {
    _mode = Data::SolverMode::simulation;
  }

  _workerMutex.unlock();
  emit solverStatus(1);
}

void Worker::exportResults(const QString& savePath)
{
  // if (_solver == nullptr) {
  //   emit errorSignal("Start the solver before exporting results!");
  //   return;
  // }
  // std::ofstream output(savePath.toStdString());
  //_workerMutex.lock();
  // Data::Exporter(*_solver, output).print();
  //_workerMutex.unlock();
}

void Worker::loadFitPlotData()
{
  if (_solver.solver == nullptr || _mode != Data::SolverMode::fitting) {
    emit errorSignal("Wrong solver mode (There is some bug!)");
  }
  _workerMutex.lock();
  if (auto* fitSolver = dynamic_cast<Fitting*>(_solver.solver.get()); fitSolver != nullptr) { fitSolver->fit(); }
  _workerMutex.unlock();
}

void Worker::loadSimPlotData()
{ // make nicer later
  //  if (_solver == nullptr || _mode != Data::SolverMode::simulation) {
  //    emit errorSignal("Wrong solver mode (there is some bug in the code)");
  //  }
}

void Worker::loadPolarPlotData()
{
  if (_solver.solver == nullptr || _mode != Data::SolverMode::simulation) {
    emit errorSignal("Wrong solver mode (there is some bug in the code)");
  }
  _workerMutex.lock();
  if (auto* simSolver = dynamic_cast<Simulation*>(_solver.solver.get()); simSolver != nullptr) {
    simSolver->calculateEmissionSubstrate();
  }
  _workerMutex.unlock();
}

Data::SolverMode Worker::getMode() { return _mode; }

bool Worker::solverAvail()
{
  if (_solver.solver == nullptr) return false;
  else return true;
}

ThreadManager::ThreadManager(const QString& configFilepath, QObject* parent) :
  QObject(parent)
{
  worker = new Worker(configFilepath);
  worker->moveToThread(&_workerThread);
  connect(&_workerThread, &QThread::finished, worker, &QObject::deleteLater);
  _workerThread.start();
  worker->startSolver();
}

ThreadManager::~ThreadManager()
{
  _workerThread.quit();
  _workerThread.wait();
}

QFrame* ThreadManager::makePlot(bool polarFlag)
{
  if (!worker->solverAvail()) {
    emit errorSignal("Start the solver before trying to plot!");
    return nullptr;
  }

  if (worker->getMode() == Data::SolverMode::fitting) {
    auto plot = new QwtPlot();
    worker->loadFitPlotData();

    const Eigen::Index N = worker->_solver.solver->resultTree.get<Vector>("angle_exp").size();
    QVector<double> x(N), yExp(N), yFit(N);

    Eigen::Map<Vector>(x.data(), N) = worker->_solver.solver->resultTree.get<Vector>("angle_exp");
    Eigen::Map<Vector>(yExp.data(), N) = worker->_solver.solver->resultTree.get<Vector>("I_angle_exp");
    Eigen::Map<Vector>(yFit.data(), N) = worker->_solver.solver->resultTree.get<Vector>("I_angle_fit");

    plot->setTitle("Fitting Results");
    plot->setCanvas(new QwtPlotCanvas());
    plot->setCanvasBackground(Qt::white);
    plot->setAxisTitle(QwtPlot::xBottom, "Angle");
    plot->setAxisTitle(QwtPlot::yLeft, "Intensity");

    QwtPlotCurve* expCurve = new QwtPlotCurve("Exp");
    QwtSymbol* symbol = new QwtSymbol(QwtSymbol::Ellipse, QBrush(Qt::blue), QPen(Qt::black), QSize(8, 8));
    expCurve->setSymbol(symbol);
    expCurve->setStyle(QwtPlotCurve::NoCurve); // No connecting line
    expCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, true);
    expCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, false);
    expCurve->setSamples(x, yExp);
    expCurve->attach(plot);

    QwtPlotCurve* fitCurve = new QwtPlotCurve("Fit");
    fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
    fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
    fitCurve->setSamples(x, yFit);
    fitCurve->attach(plot);

    QwtPlotZoomer* zoomer = new QwtPlotZoomer(plot->canvas());
    zoomer->setRubberBandPen(QColor(Qt::red));
    zoomer->setTrackerPen(QColor(Qt::blue));

    QwtLegend* legend = new QwtLegend();
    plot->insertLegend(legend);

    return plot;
  }
  else {
    if (!polarFlag) {
      auto plot = new QwtPlot();
      worker->loadSimPlotData();

      size_t dipoleLayer = worker->_solver.solver->getDipoleIndex() - 1;
      const Eigen::Index N = worker->_solver.solver->resultTree.get<Vector>("u").rows();
      QVector<double> u(N), powerPerp(N), powerParaUs(N), powerParaUp(N);

      Eigen::Map<Vector>(u.data(), N) = worker->_solver.solver->resultTree.get<Vector>("u");
      Eigen::Map<Vector>(powerPerp.data(), N) =
        worker->_solver.solver->resultTree.get<Matrix>("P_perp_uf").row(toIndex(dipoleLayer));
      Eigen::Map<Vector>(powerParaUp.data(), N) =
        worker->_solver.solver->resultTree.get<Matrix>("P_para_p_uf").row(toIndex(dipoleLayer));
      Eigen::Map<Vector>(powerParaUs.data(), N) =
        worker->_solver.solver->resultTree.get<Matrix>("P_para_s_uf").row(toIndex(dipoleLayer));

      plot->setTitle("Simulation Results");
      plot->setCanvas(new QwtPlotCanvas());
      plot->setCanvasBackground(Qt::white);
      plot->setAxisTitle(QwtPlot::xBottom, "In-plane wavevector");
      plot->setAxisTitle(QwtPlot::yLeft, "Dissipated Power (norm.)");
      plot->setAxisScaleEngine(QwtPlot::yLeft, new QwtLogScaleEngine());
      const double yMin = 1e-7;
      double yMax = std::max({*std::max_element(powerParaUs.constBegin(), powerParaUs.constEnd()),
        *std::max_element(powerParaUp.constBegin(), powerParaUp.constEnd()),
        *std::max_element(powerPerp.constBegin(), powerPerp.constEnd())});
      plot->setAxisScale(QwtPlot::yLeft, yMin, yMax);

      QwtPlotCurve* paraUsCurve = new QwtPlotCurve("s-Para");
      paraUsCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
      paraUsCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
      paraUsCurve->setPen(QPen(Qt::red));
      paraUsCurve->setSamples(u, powerParaUs);
      paraUsCurve->attach(plot);

      QwtPlotCurve* paraUpCurve = new QwtPlotCurve("p-Para");
      paraUpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
      paraUpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
      paraUpCurve->setPen(QPen(Qt::blue));
      paraUpCurve->setSamples(u, powerParaUp);
      paraUpCurve->attach(plot);

      QwtPlotCurve* perpCurve = new QwtPlotCurve("(p)-Perp");
      perpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
      perpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
      perpCurve->setPen(QPen(Qt::green));
      perpCurve->setSamples(u, powerPerp);
      perpCurve->attach(plot);

      QwtPlotZoomer* zoomer = new QwtPlotZoomer(plot->canvas());
      zoomer->setRubberBandPen(QColor(Qt::red));
      zoomer->setTrackerPen(QColor(Qt::blue));
      QwtLegend* legend = new QwtLegend();
      plot->insertLegend(legend);

      return plot;
    }
    else {
      QwtPolarPlot* polarPlot = new QwtPolarPlot();
      polarPlot->setAutoReplot(false);
      polarPlot->setAzimuthOrigin(M_PI_2);
      polarPlot->setScale(QwtPolar::Azimuth, 0.0, 360.0, 30.0); // major tick each 30°
      polarPlot->setScaleMaxMinor(QwtPolar::Azimuth, 2);
      polarPlot->setScale(QwtPolar::Radius, 0.0, 0.6); // VERY IMPORTANT
      auto zoomer = new QwtPolarMagnifier(polarPlot->canvas());

      worker->loadPolarPlotData();

      PolarData* perpPoints = new PolarData();
      PolarData* paraPPoints = new PolarData();
      PolarData* paraSPoints = new PolarData();

      const Vector& theta = worker->_solver.solver->resultTree.get<Vector>("angle");
      const Vector& IPerp = worker->_solver.solver->resultTree.get<Vector>("P_perp_sub");
      const Vector& IParaP = worker->_solver.solver->resultTree.get<Vector>("P_para_p_sub");
      const Vector& IParaS = worker->_solver.solver->resultTree.get<Vector>("P_para_s_sub");
      const Eigen::Index N = theta.size();

      auto toDeg = [](double rad) { return rad * 180.0 / M_PI; };

      for (Eigen::Index i = 0; i < N; ++i) {
        const double angle = toDeg(theta[i]);

        perpPoints->pts.push_back(QwtPointPolar(angle, IPerp[i]));
        paraPPoints->pts.push_back(QwtPointPolar(angle, IParaP[i]));
        paraSPoints->pts.push_back(QwtPointPolar(angle, IParaS[i]));
      }
      for (Eigen::Index i = 0; i < N; ++i) {
        const double angleMir = 360 - toDeg(theta[i]);

        perpPoints->pts.push_back(QwtPointPolar(angleMir, IPerp[i]));
        paraPPoints->pts.push_back(QwtPointPolar(angleMir, IParaP[i]));
        paraSPoints->pts.push_back(QwtPointPolar(angleMir, IParaS[i]));
      }

      QwtPolarCurve* perpCurve = new QwtPolarCurve("Perp");
      perpCurve->setLegendAttribute(QwtPolarCurve::LegendShowSymbol, false);
      perpCurve->setLegendAttribute(QwtPolarCurve::LegendShowLine, true);
      perpCurve->setPen(QPen(Qt::blue));
      perpCurve->setData(perpPoints);
      perpCurve->attach(polarPlot);

      QwtPolarCurve* paraPCurve = new QwtPolarCurve("p-Para");
      paraPCurve->setLegendAttribute(QwtPolarCurve::LegendShowSymbol, false);
      paraPCurve->setLegendAttribute(QwtPolarCurve::LegendShowLine, true);
      paraPCurve->setPen(QPen(Qt::red));
      paraPCurve->setData(paraPPoints);
      paraPCurve->attach(polarPlot);

      QwtPolarCurve* paraSCurve = new QwtPolarCurve("s-Para");
      paraSCurve->setLegendAttribute(QwtPolarCurve::LegendShowSymbol, false);
      paraSCurve->setLegendAttribute(QwtPolarCurve::LegendShowLine, true);
      paraSCurve->setPen(QPen(Qt::green));
      paraSCurve->setData(paraSPoints);
      paraSCurve->attach(polarPlot);

      QwtPolarGrid* grid = new QwtPolarGrid();
      grid->setPen(QPen(Qt::gray));
      grid->attach(polarPlot);

      QwtLegend* legend = new QwtLegend();
      polarPlot->insertLegend(legend);

      return polarPlot;
    }
  }
}
