#pragma once

#include <basesolver.hpp>
#include <fitting.hpp>
#include <simulation.hpp>
#include <indata.hpp>
#include <polymap.hpp>

#include <set>
#include <string>

#include <QString>
#include <QWidget>
#include <QTimer>
#include <QMutex>
#include <QThread>
#include <QPair>
#include <QwtPlot>
#include <qwt_series_data.h>
#include <qwt_point_polar.h>

namespace UIthreading {

    //workers
    class Worker : public QObject {
        Q_OBJECT
        friend class ThreadManager;
        static std::set<QString> _blacklist;
        std::unique_ptr<BaseSolver> _solver;
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

    class ThreadManager : public QObject {
        Q_OBJECT
        QThread _workerThread;

        void init();
        public:
            Worker *worker;
            
            ThreadManager(const QString& configFilepath, QObject* parent=nullptr);
            ~ThreadManager();

            QFrame* makePlot(bool polarFlag);
            
        signals:
            void solverStatus(bool status);
            void errorSignal(const QString errorString);
    };

    class PolarData : public QwtSeriesData<QwtPointPolar>
    {
        public:
            PolarData();
            PolarData(const QVector<QwtPointPolar>& points);

            virtual size_t size() const override;
            virtual QwtPointPolar sample(size_t i) const override;
            virtual QRectF boundingRect() const override;
            void push_back(QwtPointPolar elem);
            
        private:
            QVector<QwtPointPolar> _points;
    };
}