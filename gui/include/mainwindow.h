#pragma once

#include <QAction>
#include <QEvent>
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QProgressBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_zoomer.h>

#include <previewtab.h>
#include <threadhelper.h>

class MainWindow : public QMainWindow // true main window
{
protected:
  Q_OBJECT

  void createMenus();
  void createToolbar();
  void createWorkspace();
  void createCentralWidget();
  void createStatusBar();

  // Solver status and enablin/disabling actions
  void setUIRunning(bool running);
  void processWorkerSignals();

  QLabel* _workspacePathLabel;
  QString _workspaceDir;
  QTabWidget* _centralStack;
  QVBoxLayout* _previewLayout;
  UIthreading::ThreadManager* _thread;
  bool _plotStatus;

  // Solver status bar
  QLabel* _solverStatusLabel = nullptr;
  QProgressBar* _solverProgress = nullptr;

  // Actions to enable/disable
  QAction* _runAction = nullptr;
  QAction* _importAction = nullptr;
  QAction* _loadAction = nullptr;
  QAction* _fitPlotAction = nullptr;
  QAction* _dissPlotAction = nullptr;
  QAction* _polarPlotAction = nullptr;

signals:
  void workspaceChanged(const QString& dir);

public:
  MainWindow();

  void resetJob(const QString& configFilepath);

protected slots:
  void onLoad();
  void onExit();

  void displayPlot(Data::SolverMode calledMode);
  void displayPolarPlot();

  void onChangeWorkspace();
};