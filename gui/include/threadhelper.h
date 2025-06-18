#pragma once

#include <basesolver.hpp>
#include <fitting.hpp>
#include <simulation.hpp>
#include <indata.hpp>

#include <set>
#include <string>

#include <QString>
#include <QWidget>
#include <QTimer>
#include <QMutex>
#include <QThread>
#include <QPair>
#include <QwtPlot>

namespace UIthreading {

    //workers
    class Worker : public QObject {
        Q_OBJECT
        static std::set<QString> _blacklist;
        std::unique_ptr<BaseSolver> _solver;
        QString _filepath;
        Data::SolverMode _mode;
        QMutex _workerMutex;

        public slots:
            void startSolver();
            void restartSolver();
            void restartSolver(const QString& solverPath);
            void exportResults(const QString& savePath);

        signals:
            void solverStatus(bool status);
            void errorSignal(const QString errorString);

        public:
            Worker(const QString& filepath);
            Fitting::FitRes getFitPlotData();
            Simulation::SimRes getSimPlotData();
            Data::SolverMode getMode();
            bool solverAvail();
    };

    class ThreadManager : public QObject {
        Q_OBJECT
        QThread _workerThread;
        Worker *_worker;

        void init();
        public:
            ThreadManager(const QString& configFilepath, QObject* parent=nullptr);
            ~ThreadManager();

            QwtPlot* makePlot(bool polarFlag);

        public slots:
            void solverStatusRelay(bool status);
            
        signals:
            void solverStatus(bool status);
            void errorSignal(const QString errorString);
    };
}