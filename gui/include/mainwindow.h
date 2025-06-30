#pragma once

#include <QAction>
#include <QEvent>
#include <QList>
#include <QMainWindow>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_zoomer.h>

#include <previewtab.h>
#include <threadhelper.h>

class MonitoredTab;

class MainWindow : public QMainWindow // true main window
{
protected:
  Q_OBJECT

  void createMenus();
  void createToolbar();
  void createPreviewTabs();
  void createCentralWidget();
  void createCanvas();

  QList<MonitoredTab*> _tabList;
  QStackedWidget* _centralStack;
  MonitoredTab* _currentTab;
  QVBoxLayout* _previewLayout;
  bool _plotStatus;

public:
  MainWindow();

  PreviewTab* getPreviewTab();
  void refreshPreviewTab(MonitoredTab* tab); // safe(r) tab refresh

  void newCurrentBlankTab(const QString& label = "");
  void newCurrentTabFromFile(const QString& configFilepath, const QString& label = "");

protected slots:
  void onOpen();
  void onNewTab();
  void onLoad();
  void onReload();
  void onExit();
  void onSave();

  void savePlot();
  void displayPlot(Data::SolverMode calledMode);
  void displayPolarPlot();
  void deletePlot();
};

class MonitoredTab : public QWidget
{
  Q_OBJECT

  PreviewTab* _previewTab; // keeps previewtab reference in raw pointer
  UIthreading::ThreadManager* _thread;
  QVBoxLayout* _layout;
  bool _plotAvail;

  friend class MainWindow;

public:
  MonitoredTab() = delete; // helps avoid memory leaks
  MonitoredTab(QWidget* parent = nullptr);
  MonitoredTab(QString& configFilepath, QWidget* parent = nullptr);

  void setPreviewTab(PreviewTab* tab);

  void makeJob(const QString& configFilepath);
  void resetJob(const QString& configFilepath);
  void resetJob();

  bool plotAvail();
  void makeCanvas();
  void setPlot(bool polarFlag);
  void saveToFile(const QString& savePath);

  QwtPlot* plot;

protected:
  void changeEvent(QEvent* event) override;
  void showEvent(QShowEvent* event) override;
};