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
#include <QPair>
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
            void solverStatus(bool status);
            void errorSignal(const QString errorString);

        public:
            Worker(std::string& filepath);

            Fitting::FitRes getFitPlotData();
            Simulation::SimRes getSimPlotData();
            Data::SolverMode getMode();
            bool solverAvail();
    };

    class ThreadManager : public QObject {
        Q_OBJECT
        QThread _workerThread;
        Worker *_worker;
        QwtPlot *_plot;

        void init();
        public:
            ThreadManager(std::string& configFilepath);
            ~ThreadManager();

            QwtPlot* makePlot(bool polarFlag);

        public slots:
            void solverStatusRelay(bool status);
            
        signals:
            void solverStatus(bool status);
            void errorSignal(const QString errorString);
    };
}