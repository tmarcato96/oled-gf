#include <threadhelper.h>

#include <set>
#include <string>
#include <fstream>

#include <indata.hpp>
#include <outdata.hpp>
#include <basesolver.hpp>
#include <fitting.hpp>
#include <simulation.hpp>

#include <QWidget>
#include <QTimer>
#include <QMutex>

#include <QwtPlot>

using namespace UImethods;

void Worker::startSolver() {
    _workerMutex.lock();
    if (_blacklist.find(_filepath) != _blacklist.end()) {
        emit errorSignal("Solver already exists!");
        return;
    }
    _blacklist.insert(_filepath);
    auto importer = Data::ImportManager(_filepath).makeImporter();
    _solver = importer->solverFromFile(); //heavier computations

    if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
    else { _mode = Data::SolverMode::simulation;}

    emit solverReady(_mode);
    _workerMutex.unlock();
}

void Worker::restartSolver() {
    _workerMutex.lock();
    if (_blacklist.find(_filepath) == _blacklist.end()) {
        emit errorSignal("Start the solver first before attempting to restart!");
        return;
    }
    auto importer = Data::ImportManager(_filepath).makeImporter();
    _solver = importer->solverFromFile(); //heavier computations

    if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
    else { _mode = Data::SolverMode::simulation;}

    emit solverReady(_mode);
    _workerMutex.unlock();
}

void Worker::restartSolver(const std::string& solverPath) {
    _workerMutex.lock();
    if (_blacklist.find(_filepath) == _blacklist.end()) {
        emit errorSignal("Start the solver first before attempting to restart!");
        return;
    }
    _blacklist.erase(_filepath);
    _filepath = solverPath;
    _blacklist.insert(_filepath);

    auto importer = Data::ImportManager(_filepath).makeImporter();
    _solver = importer->solverFromFile(); //heavier computations

    if (importer->getSolverMode() == Data::SolverMode::fitting) _mode = Data::SolverMode::fitting;
    else { _mode = Data::SolverMode::simulation;}

    emit solverReady(_mode);
    _workerMutex.unlock();
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

void Worker::plotResults(bool plotFlag) {
    if(!(solverAvail())) {
        emit errorSignal("Solver not ready");
        return;
    }
    emit plotReady(plotFlag);
}


Data::SolverMode Worker::getMode() {
    return _mode;
}

bool Worker::solverAvail() {
    if (_solver == nullptr) return 0;
    else {return 1;}
}