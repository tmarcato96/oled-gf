#pragma once

#include <QAction>
#include <QEvent>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QMainWindow>
#include <QProgressBar>
#include <QStackedWidget>
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

  void ensurePlotCreated(int row);

  // Solver status and enablin/disabling actions
  void setUIRunning(bool running);
  void processWorkerSignals();

  QLabel* _workspacePathLabel;
  QString _workspaceDir;
  QTabWidget* _centralStack;
  QWidget* _resultsPage = nullptr;
  QListWidget* _resultsList = nullptr;
  QStackedWidget* _resultsPlots = nullptr;
  QVBoxLayout* _previewLayout;
  UIthreading::ThreadManager* _thread;
  bool _plotStatus;

  // Plot widgets
  QWidget* _plotDiss = nullptr;
  QWidget* _plotPolar = nullptr;
  QWidget* _plotFit = nullptr;
  QWidget* _plotMode = nullptr;

  // Solver status bar
  QLabel* _solverStatusLabel = nullptr;
  QProgressBar* _solverProgress = nullptr;

  // Actions to enable/disable
  QAction* _runAction = nullptr;
  QAction* _importAction = nullptr;
  QAction* _loadAction = nullptr;

signals:
  void workspaceChanged(const QString& dir);

public:
  MainWindow();

  void resetJob(const QString& configFilepath);

protected slots:
  void onLoad();
  void onExit();

  void displayResultWindow();

  void onChangeWorkspace();
};