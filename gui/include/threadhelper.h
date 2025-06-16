#pragma once

#include <indata.hpp>
#include <basesolver.hpp>
#include <fitting.hpp>


#include <set>
#include <string>

#include <QString>
#include <QWidget>
#include <QTimer>
#include <QMutex>
#include <QThread>

#include <QwtPlot>

namespace UImethods {

    //workers
    class Worker : public QObject {
        Q_OBJECT
        static std::set<std::string> _blacklist;
        std::unique_ptr<BaseSolver> _solver;
        std::string _filepath;
        Data::SolverMode _mode;
        QMutex _workerMutex;

        public slots:
            void startSolver();
            void restartSolver();
            void restartSolver(const std::string& solverPath);
            void exportResults(const QString& savePath);

        signals:
            void plotterReady(bool plotFlag);
            void solverReady(const Data::SolverMode SolverMode);
            void errorSignal(const QString errorString);

        public:
            Fitting::FitRes getFitPlotData(bool plotFlag);
            Simulation::SimRes getSimPlotData(bool plotFlag);
            Data::SolverMode getMode();
            bool solverAvail();
    };

    class ThreadManager : public QObject {
        Q_OBJECT
        QThread workerThread;
        Worker *worker;
        QwtPlot *plot;

        public:
            ThreadManager() {
                worker = new Worker;
                worker->moveToThread(&workerThread);
                connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
                connect(this, &ThreadManager::operate, worker, &Worker::startSolver);
                connect(worker, &Worker::solverReady, this, &ThreadManager::handleResults);
                connect(worker, &Worker::plotterReady, this, &ThreadManager::makePlot);
                workerThread.start();
            }
            ~ThreadManager() {
                workerThread.quit();
                workerThread.wait();
            }
        public slots:
            void handleResults(const QString &);
            void makePlot(bool plotFlag);
            
        signals:
            void makePlotter();
            void operate(const QString &);
            void errorSignal(const QString errorString);
    };
}