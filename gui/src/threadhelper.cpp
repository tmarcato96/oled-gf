#define _USE_MATH_DEFINES
#include "threadhelper.h"

#include <set>
#include <string>
#include <fstream>

#include <indata.hpp>
#include <outdata.hpp>
#include <basesolver.hpp>
#include <fitting.hpp>
#include <simulation.hpp>
#include <polymap.hpp>
#include <Eigen/Core>

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

#include <qwt_point_polar.h>
#include <qwt_polar_curve.h>
#include <qwt_polar_canvas.h>
#include <qwt_polar_grid.h>
#include <qwt_polar_marker.h>
#include <qwt_polar_renderer.h>
#include <qwt_series_data.h>


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

void Worker::loadFitPlotData() {
    if(_solver == nullptr || _mode != Data::SolverMode::fitting) {
        emit errorSignal("Wrong solver mode (there is some bug in the code)");
    }
    _workerMutex.lock();
    auto *fitSolver = dynamic_cast<Fitting*>(_solver.get());
    fitSolver->fitEmissionSubstrate();
    _workerMutex.unlock();
}

void Worker::loadSimPlotData() { //make nicer later
    if(_solver == nullptr || _mode != Data::SolverMode::simulation) {
        emit errorSignal("Wrong solver mode (there is some bug in the code)");
    }
}

void Worker::loadPolarPlotData() {
    if(_solver == nullptr || _mode != Data::SolverMode::simulation) {
        emit errorSignal("Wrong solver mode (there is some bug in the code)");
    }
    _workerMutex.lock();
    auto *simSolver = dynamic_cast<Simulation*>(_solver.get());
    simSolver->calculateEmissionSubstrate();
    _workerMutex.unlock();
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

QFrame* ThreadManager::makePlot(bool polarFlag) {
    if(!worker->solverAvail()) {
        emit errorSignal("Start the solver before trying to plot!");
        return nullptr;
    }
    
    if(worker->getMode() == Data::SolverMode::fitting) {
        auto plot = new QwtPlot();
        worker->loadFitPlotData();

        Vector& eigenTheta = worker->_solver->resMap.get<Vector>("theta");
        Vector& eigenFit = worker->_solver->resMap.get<Vector>("yFit");
        Vector& eigenExp = worker->_solver->resMap.get<Vector>("yExp");

        QVector<double> theta(eigenTheta.data(), eigenTheta.data() + eigenTheta.size());
        QVector<double> yExp(eigenFit.data(), eigenFit.data() + eigenFit.size());
        QVector<double> yFit(eigenFit.data(), eigenFit.data() + eigenFit.size());  
                
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
        scatterCurve->setSamples(theta, yExp);
        scatterCurve->attach(plot);


        QwtPlotCurve *fitCurve = new QwtPlotCurve("Fit");
        fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
        fitCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
        fitCurve->setSamples(theta, yFit);
        fitCurve->attach(plot);

        QwtPlotZoomer *zoomer = new QwtPlotZoomer(plot->canvas());
        zoomer->setRubberBandPen(QColor(Qt::red));
        zoomer->setTrackerPen(QColor(Qt::blue));
        QwtLegend *legend = new QwtLegend();
        plot->insertLegend(legend);

        return plot;
    }        
    else{
        if(!polarFlag) {
            auto plot = new QwtPlot();
            worker->loadSimPlotData();

            Vector& eigenU = worker->_solver->resMap.get<Vector>("u");
            Matrix& eigenParaS = worker->_solver->resMap.get<Matrix>("fpParaS");
            Matrix& eigenParaP = worker->_solver->resMap.get<Matrix>("fpParaP");
            Matrix& eigenPerp = worker->_solver->resMap.get<Matrix>("fpPerpP");
            Eigen::Index& dipoleLayer = worker->_solver->resMap.get<Eigen::Index>("dLayer");

            //some pointer arithmetic
            QVector<double> u(eigenU.data(), eigenU.data() + eigenU.size());
            QVector<double> fpParaS(eigenParaS.row(dipoleLayer).data(), eigenParaS.data() + eigenParaS.row(dipoleLayer).size());
            QVector<double> fpParaP(eigenParaP.row(dipoleLayer).data(), eigenParaP.data() + eigenParaP.row(dipoleLayer).size());
            QVector<double> fpPerpP(eigenPerp.row(dipoleLayer).data(), eigenPerp.data() + eigenPerp.row(dipoleLayer).size());

            plot->setTitle("Simulation Results");
            plot->setCanvas(new QwtPlotCanvas());
            plot->setCanvasBackground(Qt::white);
            plot->setAxisTitle(QwtPlot::xBottom, "X");
            plot->setAxisTitle(QwtPlot::yLeft, "Y");

            QwtPlotCurve *paraUsCurve = new QwtPlotCurve("s-Para");
            paraUsCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
            paraUsCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
            paraUsCurve->setPen(QPen(Qt::red));
            paraUsCurve->setSamples(u, fpParaS);
            paraUsCurve->attach(plot);
            
            QwtPlotCurve *paraUpCurve = new QwtPlotCurve("p-Para");
            paraUpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
            paraUpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
            paraUpCurve->setPen(QPen(Qt::blue));
            paraUpCurve->setSamples(u, fpParaP);
            paraUpCurve->attach(plot);

            QwtPlotCurve *perpCurve = new QwtPlotCurve("(p)-Perp");
            perpCurve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol, false);
            perpCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine, true);
            perpCurve->setPen(QPen(Qt::green));
            perpCurve->setSamples(u, fpPerpP);
            perpCurve->attach(plot);

            QwtPlotZoomer *zoomer = new QwtPlotZoomer(plot->canvas());
            zoomer->setRubberBandPen(QColor(Qt::red));
            zoomer->setTrackerPen(QColor(Qt::blue));
            QwtLegend *legend = new QwtLegend();
            plot->insertLegend(legend);

            return plot;
        }
        else{
            QwtPolarPlot *polarPlot = new QwtPolarPlot();
            polarPlot->setAzimuthOrigin(M_PI/2);
            worker->loadPolarPlotData();

            //polar plot does not follow the same structure as QwtPlot
            PolarData *paraSPoints = new PolarData();
            PolarData *paraPPoints = new PolarData();
            PolarData *perpPPoints = new PolarData();

            Vector& theta = worker->_solver->resMap.get<Vector>("theta");
            Vector& pParaSSub = worker->_solver->resMap.get<Vector>("pParaSSub");
            Vector& pParaPSub = worker->_solver->resMap.get<Vector>("pParaPSub");
            Vector& pPerpPSub = worker->_solver->resMap.get<Vector>("pPerpPSub");

            for(ptrdiff_t i = 0; i < theta.size(); i++) {
                double angle = theta[i]*180/M_PI;

                paraSPoints->push_back({angle, pParaSSub[i]});
                paraPPoints->push_back({angle, pParaPSub[i]});
                perpPPoints->push_back({angle, pPerpPSub[i]});
            }

            QwtPolarCurve *paraUsCurve = new QwtPolarCurve("s-Para");
            paraUsCurve->setLegendAttribute(QwtPolarCurve::LegendShowSymbol, false);
            paraUsCurve->setLegendAttribute(QwtPolarCurve::LegendShowLine, true);
            paraUsCurve->setPen(QPen(Qt::red));
            paraUsCurve->setData(paraSPoints);
            paraUsCurve->attach(polarPlot);
    
            QwtPolarCurve *paraUpCurve = new QwtPolarCurve("p-Para");
            paraUpCurve->setLegendAttribute(QwtPolarCurve::LegendShowSymbol, false);
            paraUpCurve->setLegendAttribute(QwtPolarCurve::LegendShowLine, true);
            paraUpCurve->setPen(QPen(Qt::green));
            paraUpCurve->setData(paraPPoints);
            paraUpCurve->attach(polarPlot);

            QwtPolarCurve *perpCurve = new QwtPolarCurve("(p)-Perp");
            perpCurve->setLegendAttribute(QwtPolarCurve::LegendShowSymbol, false);
            perpCurve->setLegendAttribute(QwtPolarCurve::LegendShowLine, true);
            perpCurve->setPen(QPen(Qt::blue));
            perpCurve->setData(perpPPoints);
            perpCurve->attach(polarPlot);

            QwtPolarGrid* grid = new QwtPolarGrid();
            grid->setPen(QPen(Qt::gray));
            grid->attach(polarPlot);

            return polarPlot;
        }
    }
}

PolarData::PolarData(const QVector<QwtPointPolar>& points)
    : _points{points}
    {}

PolarData::PolarData()
    : _points{}
    {}

size_t PolarData::size() const {return _points.size();}

QwtPointPolar PolarData::sample(size_t i) const {return _points[i];}

QRectF PolarData::boundingRect() const {return QRectF();}

void PolarData::push_back(QwtPointPolar elem) {_points.push_back(elem);}