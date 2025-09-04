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
#include <QWidget>
#include <QwtPlot>
#include <qwt_point_polar.h>
#include <qwt_series_data.h>

namespace UIthreading {

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

  public:
    Worker(const QString& filepath);

    void loadFitPlotData();
    void loadSimPlotData();
    void loadPolarPlotData();
    Data::SolverMode getMode();

    bool solverAvail();
    void startSolver();
    void restartSolver();
    void restartSolver(const QString& configFilepath);

    void exportResults(const QString& savePath);
  };

  class ThreadManager : public QObject
  {
    Q_OBJECT
    QThread _workerThread;

    void init();

  public:
    Worker* worker;

    ThreadManager(const QString& configFilepath, QObject* parent = nullptr);
    ~ThreadManager();

    QFrame* makePlot(bool polarFlag);

  signals:
    void solverStatus(bool status);
    void errorSignal(const QString errorString);
  };

} // namespace UIthreading