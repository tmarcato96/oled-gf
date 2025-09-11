#pragma once

#include <basesolver.hpp>
#include <fitting.hpp>
#include <indata.hpp>
#include <polymap.hpp>
#include <simulation.hpp>

#include <set>
#include <string>

#include <QMutex>
#include <QPair>
#include <QPen>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <QwtPlot>
#include <qwt_point_polar.h>
#include <qwt_series_data.h>

namespace UIthreading {

  struct FitPlotData
  {
    QVector<double> x, yExp, yFit;
    double fitRes;
  };
  struct DissPlotData
  {
    QVector<double> u, perp, paraP, paraS;
  };

  struct PolarPlotData
  {
    QVector<QwtPointPolar> perp, paraP, paraS;
  };

  // workers
  class Worker : public QObject
  {
    Q_OBJECT
    friend class ThreadManager;
    static std::set<QString> _blacklist;
    SolverManager _solver;
    QString _filepath;
    Data::SolverMode _mode;
    QMutex _workerMutex;

  signals:
    void solverStatus(bool status);
    void errorSignal(const QString errorString);
    void fitDataReady(FitPlotData);
    void dissDataReady(DissPlotData);
    void polarDataReady(PolarPlotData);

  public:
    Worker(const QString& filepath);

    Data::SolverMode getMode();

    bool solverAvail();
    void restartSolver();

    void exportResults(const QString& savePath);

  public slots:
    void restartSolver(const QString& configFilepath);
    void startSolver();
    void loadFitPlotData();
    void loadSimPlotData();
    void loadPolarPlotData();
  };

  class ThreadManager : public QObject
  {
    Q_OBJECT
    QThread _workerThread;

    void init();

  public:
    Worker* worker;

    void restartSolver(const QString& configFilepath);
    ThreadManager(const QString& configFilepath, QObject* parent = nullptr);
    ~ThreadManager();

    QFrame* makePlot(bool polarFlag);

  signals:
    void solverStatus(bool status);
    void errorSignal(const QString errorString);
    void requestRestart(const QString& configFilepath);
    void requestStart();
  };

} // namespace UIthreading