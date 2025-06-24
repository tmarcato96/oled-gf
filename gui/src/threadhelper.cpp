#include "threadhelper.h"

#include <set>
#include <string>
#include <fstream>

#include <indata.hpp>
#include <outdata.hpp>
#include <basesolver.hpp>
#include <fitting.hpp>
#include <simulation.hpp>

#include <QWidget>
#include <QVector>
#include <QTimer>
#include <QMutex>

#include <QwtPlot>
#include <QScrollArea>
#include <QMainWindow>
#include <QwtPlotZoomer>
#include <QPen>
#include <qwt_legend.h>
#include <qwt_plot_curve.h>
#include <qwt_symbol.h>
#include <qwt_plot_zoomer.h>
#include <qwt_plot_canvas.h>

using namespace UIthreading;

std::set<QString> Worker::_blacklist{};

Worker::Worker(const QString& filepath) :
    _filepath{filepath}
    {
        emit solverStatus(0);
    }

void Worker::startSolver() {
    _workerMutex.lock();
    emit solverStatus(0);
    if (_blacklist.find(_filepath) != _blacklist.end()) {
        emit errorSignal("Solver already exists!");
        return;
    }
    _blacklist.insert(_filepath);
    auto importer = Data::ImportManager(_filepath.toStdString()).makeImporter();
    _solver = importer->solverFromFile(); 
    _solver->run(); //heavier computations

    if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
    else { _mode = Data::SolverMode::simulation;}

    _workerMutex.unlock();
    emit solverStatus(1);
}

void Worker::restartSolver() {
    _workerMutex.lock();
    emit solverStatus(0);
    if (_blacklist.find(_filepath) == _blacklist.end()) {
        emit errorSignal("Start the solver first before attempting to restart!");
        return;
    }
    auto importer = Data::ImportManager(_filepath.toStdString()).makeImporter();
    _solver = importer->solverFromFile(); //heavier computations
    _solver->run();

    if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
    else { _mode = Data::SolverMode::simulation;}

    _workerMutex.unlock();
    emit solverStatus(1);
}

void Worker::restartSolver(const QString& solverPath) {
    _workerMutex.lock();
    if (_blacklist.find(_filepath) == _blacklist.end()) {
        emit errorSignal("Start the solver first before attempting to restart!");
        return;
    }
    emit solverStatus(0);
    _filepath = solverPath;

    auto importer = Data::ImportManager(_filepath.toStdString()).makeImporter();
    _solver = importer->solverFromFile(); 
    _solver->run();//heavier computations

    if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
    else { _mode = Data::SolverMode::simulation;}

    _workerMutex.unlock();
    emit solverStatus(1);
}

void Worker::exportResults(const QString& savePath) {
    if(_solver == nullptr) {
        emit errorSignal("Start the solver before exporting results!");
        return;
    }
    std::ofstream output(savePath.toStdString());
    _workerMutex.lock();
    Data::Exporter(*_solver, output).print();
    _workerMutex.unlock();
}

Fitting::FitRes Worker::getFitPlotData() {
    if(_solver == nullptr || _mode != Data::SolverMode::fitting) {
        emit errorSignal("Wrong solver mode (there is some bug in the code)");
    }
    _workerMutex.lock();
    auto *fitSolver = dynamic_cast<Fitting*>(_solver.get());
    Fitting::FitRes fitRes = fitSolver->fitEmissionSubstrate();
    _workerMutex.unlock();
    return fitRes;
}

Simulation::SimRes Worker::getSimPlotData() {
    if(_solver == nullptr || _mode != Data::SolverMode::simulation) {
        emit errorSignal("Wrong solver mode (there is some bug in the code)");
    }
    _workerMutex.lock();
    auto *simSolver = dynamic_cast<Simulation*>(_solver.get());
    Simulation::SimRes simRes = simSolver->powerModeDissipation();
    _workerMutex.unlock();
    return simRes;
}

Data::SolverMode Worker::getMode() {
    return _mode;
}

bool Worker::solverAvail() {
    if (_solver == nullptr) return 0;
    else {return 1;}
}

ThreadManager::ThreadManager(const QString& configFilepath, QObject* parent)
    : QObject(parent)
    {
    worker = new Worker(configFilepath);
    worker->moveToThread(&_workerThread);
    connect(&_workerThread, &QThread::finished, worker, &QObject::deleteLater);
    _workerThread.start();
    worker->startSolver();
    }

ThreadManager::~ThreadManager() {
    _workerThread.quit();
    _workerThread.wait();
}

QwtPlot* ThreadManager::makePlot(bool polarFlag) {
    if(!worker->solverAvail()) {
        emit errorSignal("Start the solver before trying to plot!");
        return nullptr;
    }
    
    auto plot = new QwtPlot();
    if(!polarFlag) {
        if(worker->getMode() == Data::SolverMode::fitting){
            auto fitData = worker->getFitPlotData();
            QVector<double> x{fitData.x.begin(), fitData.x.end()};
            QVector<double> yExp{fitData.yExp.begin(), fitData.yExp.end()};
            QVector<double> yFit{fitData.yFit.begin(), fitData.yFit.end()};
            
            if(!polarFlag) {
                plot->setTitle("Fitting Results");
                plot->setCanvas(new QwtPlotCanvas());
                plot->setCanvasBackground(Qt::white);
                plot->setAxisTitle(QwtPlot::xBottom, "X");
                plot->setAxisTitle(QwtPlot::yLeft, "Y");
                
                QwtPlotCurve *scatterCurve = new QwtPlotCurve("Exp");
                QwtSymbol *symbol = new QwtSymbol(QwtSymbol::Triangle, QBrush(Qt::blue), QPen(Qt::black), QSize(8, 8));
                scatterCurve->setSymbol(symbol);
                scatterCurve->setStyle(QwtPlotCurve::NoCurve); // No connecting line
                scatterCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, true);
                scatterCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, false);
                scatterCurve->setSamples(x, yExp);
                scatterCurve->attach(plot);


                QwtPlotCurve *fitCurve = new QwtPlotCurve("Fit");
                fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
                fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
                fitCurve->setSamples(x, yFit);
                fitCurve->attach(plot);

                QwtPlotZoomer *zoomer = new QwtPlotZoomer(plot->canvas());
                zoomer->setRubberBandPen(QColor(Qt::red));
                zoomer->setTrackerPen(QColor(Qt::blue));
                QwtLegend *legend = new QwtLegend();
                plot->insertLegend(legend);
            }
        }
        
        else{
            auto simData = worker->getSimPlotData();
            //there's no good way around this
            QVector<double> u{simData.u.begin(), simData.u.end()};
            QVector<double> yParaUs{simData.yParaUsPol.begin(), simData.yParaUsPol.end()};
            QVector<double> yParaUp{simData.yParaUpPol.begin(), simData.yParaUpPol.end()};
            QVector<double> yPerp{simData.yPerp.begin(), simData.yPerp.end()};

            plot->setTitle("Simulation Results");
            plot->setCanvas(new QwtPlotCanvas());
            plot->setCanvasBackground(Qt::white);
            plot->setAxisTitle(QwtPlot::xBottom, "X");
            plot->setAxisTitle(QwtPlot::yLeft, "Y");

            QwtPlotCurve *paraUsCurve = new QwtPlotCurve("s-Para");
            paraUsCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
            paraUsCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
            paraUsCurve->setPen(QPen(Qt::red));
            paraUsCurve->setSamples(u, yParaUs);
            paraUsCurve->attach(plot);
            
            QwtPlotCurve *paraUpCurve = new QwtPlotCurve("p-Para");
            paraUpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
            paraUpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
            paraUpCurve->setPen(QPen(Qt::blue));
            paraUpCurve->setSamples(u, yParaUp);
            paraUpCurve->attach(plot);

            QwtPlotCurve *perpCurve = new QwtPlotCurve("(p)-Perp");
            perpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
            perpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
            perpCurve->setPen(QPen(Qt::green));
            perpCurve->setSamples(u, yPerp);
            perpCurve->attach(plot);

            QwtPlotZoomer *zoomer = new QwtPlotZoomer(plot->canvas());
            zoomer->setRubberBandPen(QColor(Qt::red));
            zoomer->setTrackerPen(QColor(Qt::blue));
            QwtLegend *legend = new QwtLegend();
            plot->insertLegend(legend);
        }
    }
    else{
        return nullptr; //polar plot to be implemented soon
    }
    return plot;
}