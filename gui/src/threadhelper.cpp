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

#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPen>
#include <QScrollArea>
#include <QVBoxLayout>
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
  QRectF boundingRect() const override { return qwtBoundingRect(*this); }
};

std::set<QString> Worker::_blacklist{};

Worker::Worker(const QString& filepath) :
  _filepath{filepath}
{}

void Worker::startSolver()
{
  QMutexLocker lock(&_workerMutex);
  emit solverStatus(false);

  if (_blacklist.find(_filepath) != _blacklist.end()) {
    emit errorSignal("Solver already exists!");
    return;
  }
  _blacklist.insert(_filepath);

  try {
    auto importer = Data::ImportManager(_filepath.toStdString()).makeImporter();
    _solver = importer->solverFromFile();
    _solver.sweepManager->runSweeps(); // heavier computations

    if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
    else {
      _mode = Data::SolverMode::simulation;
    }

    emit solverStatus(true);
  } catch (const std::exception& e) {
    emit errorSignal(QString::fromUtf8(e.what()));
    emit solverStatus(true);
  } catch (...) {
    emit errorSignal(tr("Uknown error in startSolver()"));
    emit solverStatus(true);
  }
}

void Worker::restartSolver()
{
  QMutexLocker lock(&_workerMutex);
  emit solverStatus(false);

  try {
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

    emit solverStatus(true);
  } catch (const std::exception& e) {
    emit errorSignal(QString::fromUtf8(e.what()));
    emit solverStatus(true);
  } catch (...) {
    emit errorSignal(tr("Uknown error in startSolver()"));
    emit solverStatus(true);
  }
}

void Worker::restartSolver(const QString& solverPath)
{
  QMutexLocker lock(&_workerMutex);
  emit solverStatus(false);

  try {
    if (_blacklist.find(_filepath) == _blacklist.end()) {
      emit errorSignal("Start the solver first before attempting to restart!");
      return;
    }

    _filepath = solverPath;
    auto importer = Data::ImportManager(_filepath.toStdString()).makeImporter();
    _solver = importer->solverFromFile(); // heavier computations
    _solver.sweepManager->runSweeps();

    if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
    else {
      _mode = Data::SolverMode::simulation;
    }

    emit solverStatus(true);
  } catch (const std::exception& e) {
    emit errorSignal(QString::fromUtf8(e.what()));
    emit solverStatus(true);
  } catch (...) {
    emit errorSignal(tr("Uknown error in startSolver()"));
    emit solverStatus(true);
  }
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
  QMutexLocker lock(&_workerMutex);
  if (_solver.solver == nullptr || _mode != Data::SolverMode::fitting) {
    emit errorSignal("Wrong solver mode (There is some bug!)");
    return;
  }
  if (auto* fitSolver = dynamic_cast<Fitting*>(_solver.solver.get()); fitSolver != nullptr) { fitSolver->fit(); }

  const Eigen::Index N = _solver.solver->resultTree.get<Vector>("angle_exp").size();
  FitPlotData data;
  data.x.resize(N);
  data.yFit.resize(N);
  data.yExp.resize(N);

  Eigen::Map<Vector>(data.x.data(), N) = _solver.solver->resultTree.get<Vector>("angle_exp");
  Eigen::Map<Vector>(data.yExp.data(), N) = _solver.solver->resultTree.get<Vector>("I_angle_exp");
  Eigen::Map<Vector>(data.yFit.data(), N) = _solver.solver->resultTree.get<Vector>("I_angle_fit");

  data.fitRes = _solver.solver->resultTree.get<double>("dipole_orientation_fit");

  emit fitDataReady(std::move(data));
}

void Worker::loadSimPlotData()
{
  QMutexLocker lock(&_workerMutex);
  if (_solver.solver == nullptr || _mode != Data::SolverMode::simulation) {
    emit errorSignal("Wrong solver mode (There is some bug!)");
    return;
  }

  size_t dipoleLayer = _solver.solver->getDipoleIndex() - 1;
  const Eigen::Index N = _solver.solver->resultTree.get<Vector>("u").rows();
  DissPlotData data;
  data.u.resize(N);
  data.perp.resize(N);
  data.paraP.resize(N);
  data.paraS.resize(N);

  Eigen::Map<Vector>(data.u.data(), N) = _solver.solver->resultTree.get<Vector>("u");
  Eigen::Map<Vector>(data.perp.data(), N) =
    _solver.solver->resultTree.get<Matrix>("P_perp_uf").row(toIndex(dipoleLayer));
  Eigen::Map<Vector>(data.paraP.data(), N) =
    _solver.solver->resultTree.get<Matrix>("P_para_p_uf").row(toIndex(dipoleLayer));
  Eigen::Map<Vector>(data.paraS.data(), N) =
    _solver.solver->resultTree.get<Matrix>("P_para_s_uf").row(toIndex(dipoleLayer));

  emit dissDataReady(std::move(data));
}

void Worker::loadPolarPlotData()
{
  QMutexLocker lock(&_workerMutex);
  if (_solver.solver == nullptr || _mode != Data::SolverMode::simulation) {
    emit errorSignal("Wrong solver mode (there is some bug in the code)");
    return;
  }
  if (auto* simSolver = dynamic_cast<Simulation*>(_solver.solver.get()); simSolver != nullptr) {
    simSolver->calculateEmissionSubstrate();
  }

  PolarPlotData data;

  const Vector& theta = _solver.solver->resultTree.get<Vector>("angle");
  const Vector& IPerp = _solver.solver->resultTree.get<Vector>("P_perp_sub");
  const Vector& IParaP = _solver.solver->resultTree.get<Vector>("P_para_p_sub");
  const Vector& IParaS = _solver.solver->resultTree.get<Vector>("P_para_s_sub");
  const Eigen::Index N = theta.size();

  auto safeMaxCoeff = [](const Vector& v) {
    double maxVal = -std::numeric_limits<double>::infinity();
    for (Eigen::Index i = 0; i < v.size(); ++i) {
      const double val = v[i];
      if (std::isfinite(val)) {
        if (val > maxVal) maxVal = val;
      }
    }
    return (maxVal == -std::numeric_limits<double>::infinity()) ? 0.0 : maxVal;
  };

  auto safe = [](double d) { return (d > 0) ? d : 1.0; };

  const double maxPerp = safeMaxCoeff(IPerp);
  const double maxParaP = safeMaxCoeff(IParaP);
  const double maxParaS = safeMaxCoeff(IParaS);

  data.perp.reserve(2 * N);
  data.paraP.reserve(2 * N);
  data.paraS.reserve(2 * N);

  auto toDeg = [](double rad) { return rad * 180.0 / M_PI; };

  for (Eigen::Index i = 0; i < N; ++i) {
    const double angle = toDeg(theta[i]);

    data.perp.push_back(QwtPointPolar(angle, IPerp[i] / safe(maxPerp)));
    data.paraP.push_back(QwtPointPolar(angle, IParaP[i] / safe(maxParaP)));
    data.paraS.push_back(QwtPointPolar(angle, IParaS[i] / safe(maxParaS)));
  }
  for (Eigen::Index i = 0; i < N; ++i) {
    const double angleMir = 360 - toDeg(theta[i]);

    data.perp.push_back(QwtPointPolar(angleMir, IPerp[i] / safe(maxPerp)));
    data.paraP.push_back(QwtPointPolar(angleMir, IParaP[i] / safe(maxParaP)));
    data.paraS.push_back(QwtPointPolar(angleMir, IParaS[i] / safe(maxParaS)));
  }

  emit polarDataReady(std::move(data));
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
  connect(this, &ThreadManager::requestStart, worker, &Worker::startSolver, Qt::QueuedConnection);
  connect(
    this,
    &ThreadManager::requestRestart,
    worker,
    [this](const QString& cfg) { worker->restartSolver(cfg); },
    Qt::QueuedConnection);
  connect(worker, &Worker::solverStatus, this, &ThreadManager::solverStatus);
  connect(worker, &Worker::errorSignal, this, &ThreadManager::errorSignal, Qt::QueuedConnection);
  _workerThread.start();
  emit requestStart();
}

void ThreadManager::restartSolver(const QString& configFilePath) { emit requestRestart(configFilePath); }

ThreadManager::~ThreadManager()
{
  _workerThread.quit();
  _workerThread.wait();
}

QWidget* ThreadManager::makeFitPlot()
{
  if (worker->getMode() == Data::SolverMode::fitting) {

    auto* container = new QWidget;
    auto* hbox = new QHBoxLayout(container);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(8);

    auto plot = new QwtPlot();
    plot->setTitle("Fitting Results");
    plot->setCanvas(new QwtPlotCanvas());
    plot->setCanvasBackground(Qt::white);
    plot->setAxisTitle(QwtPlot::xBottom, "Angle");
    plot->setAxisTitle(QwtPlot::yLeft, "Intensity");

    auto* rightPanel = new QWidget;
    auto* vbox = new QVBoxLayout(rightPanel);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(6);

    auto* legend = new QwtLegend(rightPanel);
    vbox->addWidget(legend);

    auto* resultLabel = new QLabel("Fit result: -", rightPanel);
    resultLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    resultLabel->setMaximumWidth(140);
    vbox->addWidget(resultLabel);
    vbox->addStretch();

    hbox->addWidget(plot, 1);
    hbox->addWidget(rightPanel, 0);

    connect(plot, &QwtPlot::legendDataChanged, legend, &QwtLegend::updateLegend);

    QMetaObject::Connection conn;
    conn = QObject::connect(
      worker,
      &Worker::fitDataReady,
      plot,
      [plot, conn, resultLabel, legend](FitPlotData data) mutable {
        QwtPlotCurve* expCurve = new QwtPlotCurve("Exp");
        QwtSymbol* symbol = new QwtSymbol(QwtSymbol::Ellipse, QBrush(Qt::blue), QPen(Qt::black), QSize(8, 8));
        expCurve->setSymbol(symbol);
        expCurve->setStyle(QwtPlotCurve::NoCurve); // No connecting line
        expCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, true);
        expCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, false);
        expCurve->setSamples(data.x, data.yExp);
        expCurve->attach(plot);

        QwtPlotCurve* fitCurve = new QwtPlotCurve("Fit");
        fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
        fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
        fitCurve->setSamples(data.x, data.yFit);
        fitCurve->attach(plot);

        QwtPlotZoomer* zoomer = new QwtPlotZoomer(plot->canvas());
        zoomer->setRubberBandPen(QColor(Qt::red));
        zoomer->setTrackerPen(QColor(Qt::blue));

        resultLabel->setText(QString("Fit result: %1").arg(data.fitRes, 0, 'g', 6));

        plot->replot();

        QObject::disconnect(conn);
      },
      Qt::QueuedConnection);

    QMetaObject::invokeMethod(worker, "loadFitPlotData", Qt::QueuedConnection);

    return container;
  }
}

QFrame* ThreadManager::makePlot(bool polarFlag)
{
  if (!worker->solverAvail()) {
    emit errorSignal("Start the solver before trying to plot!");
    return nullptr;
  }

  if (!polarFlag) {
    auto plot = new QwtPlot();

    plot->setTitle("Simulation Results");
    plot->setCanvas(new QwtPlotCanvas());
    plot->setCanvasBackground(Qt::white);
    plot->setAxisTitle(QwtPlot::xBottom, "In-plane wavevector");
    plot->setAxisTitle(QwtPlot::yLeft, "Dissipated Power (norm.)");
    plot->setAxisScaleEngine(QwtPlot::yLeft, new QwtLogScaleEngine());

    QMetaObject::Connection conn;
    conn = QObject::connect(
      worker,
      &Worker::dissDataReady,
      plot,
      [plot, conn](DissPlotData data) mutable {
        double yMax = std::max({*std::max_element(data.paraS.begin(), data.paraS.end()),
          *std::max_element(data.paraP.begin(), data.paraP.end()),
          *std::max_element(data.perp.begin(), data.perp.end())});
        const double yMin = 1e-7;
        plot->setAxisScale(QwtPlot::yLeft, yMin, yMax);

        QwtPlotCurve* paraUsCurve = new QwtPlotCurve("s-Para");
        paraUsCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
        paraUsCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
        paraUsCurve->setPen(QPen(Qt::red));
        paraUsCurve->setSamples(data.u, data.paraS);
        paraUsCurve->attach(plot);

        QwtPlotCurve* paraUpCurve = new QwtPlotCurve("p-Para");
        paraUpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
        paraUpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
        paraUpCurve->setPen(QPen(Qt::blue));
        paraUpCurve->setSamples(data.u, data.paraP);
        paraUpCurve->attach(plot);

        QwtPlotCurve* perpCurve = new QwtPlotCurve("(p)-Perp");
        perpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
        perpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
        perpCurve->setPen(QPen(Qt::green));
        perpCurve->setSamples(data.u, data.perp);
        perpCurve->attach(plot);

        QwtPlotZoomer* zoomer = new QwtPlotZoomer(plot->canvas());
        zoomer->setRubberBandPen(QColor(Qt::red));
        zoomer->setTrackerPen(QColor(Qt::blue));
        QwtLegend* legend = new QwtLegend();
        plot->insertLegend(legend);

        QObject::disconnect(conn);
      },
      Qt::QueuedConnection);

    QMetaObject::invokeMethod(worker, "loadSimPlotData", Qt::QueuedConnection);

    return plot;
  }
  else {
    QwtPolarPlot* polarPlot = new QwtPolarPlot();
    polarPlot->setAzimuthOrigin(M_PI_2);
    polarPlot->setScale(QwtPolar::Azimuth, 0.0, 360.0, 30.0); // major tick each 30°
    polarPlot->setScaleMaxMinor(QwtPolar::Azimuth, 2);
    polarPlot->setScale(QwtPolar::Radius, 0.0, 1.0); // VERY IMPORTANT
    auto zoomer = new QwtPolarMagnifier(polarPlot->canvas());

    QMetaObject::Connection conn;
    conn = QObject::connect(
      worker,
      &Worker::polarDataReady,
      polarPlot,
      [polarPlot, conn](PolarPlotData data) mutable {
        PolarData* polarPerp = new PolarData();
        polarPerp->pts = data.perp;
        QwtPolarCurve* perpCurve = new QwtPolarCurve("Perp");
        perpCurve->setLegendAttribute(QwtPolarCurve::LegendShowSymbol, false);
        perpCurve->setLegendAttribute(QwtPolarCurve::LegendShowLine, true);
        perpCurve->setPen(QPen(Qt::blue));
        perpCurve->setData(polarPerp);
        perpCurve->attach(polarPlot);

        PolarData* polarParaP = new PolarData();
        polarParaP->pts = data.paraP;
        QwtPolarCurve* paraPCurve = new QwtPolarCurve("p-Para");
        paraPCurve->setLegendAttribute(QwtPolarCurve::LegendShowSymbol, false);
        paraPCurve->setLegendAttribute(QwtPolarCurve::LegendShowLine, true);
        paraPCurve->setPen(QPen(Qt::red));
        paraPCurve->setData(polarParaP);
        paraPCurve->attach(polarPlot);

        PolarData* polarParaS = new PolarData();
        polarParaS->pts = data.paraS;
        QwtPolarCurve* paraSCurve = new QwtPolarCurve("s-Para");
        paraSCurve->setLegendAttribute(QwtPolarCurve::LegendShowSymbol, false);
        paraSCurve->setLegendAttribute(QwtPolarCurve::LegendShowLine, true);
        paraSCurve->setPen(QPen(Qt::green));
        paraSCurve->setData(polarParaS);
        paraSCurve->attach(polarPlot);

        QwtPolarGrid* grid = new QwtPolarGrid();
        grid->setPen(QPen(Qt::gray));
        grid->attach(polarPlot);

        QwtLegend* legend = new QwtLegend();
        polarPlot->insertLegend(legend);

        polarPlot->replot();

        QObject::disconnect(conn);
      },
      Qt::QueuedConnection);

    QMetaObject::invokeMethod(worker, "loadPolarPlotData", Qt::QueuedConnection);

    return polarPlot;
  }
}
}
