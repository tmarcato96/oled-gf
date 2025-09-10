#pragma once

#include <QAction>
#include <QEvent>
#include <QList>
#include <QMainWindow>
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

  QTabWidget* _centralStack;
  QVBoxLayout* _previewLayout;
  UIthreading::ThreadManager* _thread;
  bool _plotStatus;

public:
  MainWindow();

  void resetJob(const QString& configFilepath);

protected slots:
  void onLoad();
  void onExit();

  void displayPlot(Data::SolverMode calledMode);
  void displayPolarPlot();
};