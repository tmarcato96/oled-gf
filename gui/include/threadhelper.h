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

  struct Exportable
  {

    virtual ~Exportable() = default;

    virtual void exportToCsv(const QString& filePath) const = 0;
  };

  struct FitPlotData : public Exportable
  {
    QVector<double> x, yExp, yFit;
    double fitRes;

    void exportToCsv(const QString& filePath) const override;
  };
  struct DissPlotData : public Exportable
  {
    QVector<double> u, perp, paraP, paraS;

    void exportToCsv(const QString& filePath) const override;
  };

  struct PolarPlotData : public Exportable
  {
    QVector<QwtPointPolar> perp, paraP, paraS;

    void exportToCsv(const QString& filePath) const override;
  };

  struct ModePlotData : public Exportable
  {
    QVector<double> outcoupling, substrate, waveguide, evanescent;

    void exportToCsv(const QString& filePath) const override;
  };

  struct RTPlotData : public Exportable
  {
    QVector<double> wvl, angle;
    QVector<QVector<double>> Rs, Rp;

    void exportToCsv(const QString& filePath) const override;
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
    void modeDataReady(ModePlotData);
    void RTDataReady(RTPlotData);

  public:
    Worker(const QString& filepath);

    Data::SolverMode getMode();

    bool solverAvail();
    void restartSolver();

  public slots:
    void restartSolver(const QString& configFilepath);
    void startSolver();
    void loadFitPlotData();
    void loadSimPlotData();
    void loadPolarPlotData();
    void loadModePlotData();
    void loadRTPlotData();
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

    QWidget* makeDissPlot();
    QWidget* makePolarPlot();
    QWidget* makeFitPlot();
    QWidget* makeModePlot();
    QWidget* makeRTPlot();

    void restartSolver(const QString& configFilePath);

  signals:
    void solverStatus(bool status);
    void errorSignal(const QString errorString);
    void requestStart();
    void requestRestart(const QString& configFilePath);
  };

} // namespace UIthreading