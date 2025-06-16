#pragma once

#include <QMainWindow>
#include <QAction>
#include <QEvent>
#include <QList>
#include <QVBoxLayout>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <QwtPlotZoomer>

#include <previewtab.h>

class QwtPlot;
class QwtPlotCurve;

class MainWindow : public QMainWindow //true main window
{
    Q_OBJECT

public:
    MainWindow();
    void newTab(QMainWindow* subWindow, const QString& label);

private slots:
    void onExit();
    void onSave();

private:
    void setupPlot(QwtPlot* plot);
    void createMenus();
    void createToolbar();
    void createPreviewTabs();

    QwtPlot *plot;
    QwtPlotCurve *curve;
    QwtPlotZoomer *zoomer;

    QTabWidget* _tabWidget;

    QAction *exitAction;
    QAction *saveAction;

    QList<QMainWindow*> monitoredWindowsList;
    QVBoxLayout* _previewLayout;
};

class MonitoredWindow : public QMainWindow
{
    Q_OBJECT

    public:
        MonitoredWindow(QWidget* parent = nullptr);
        void setPreviewTab(PreviewTab* tab);

    protected:
        void changeEvent(QEvent* event) override;
        void showEvent(QShowEvent* event) override;
    private:
        PreviewTab* _previewTab; //keeps previewtab reference in raw pointer
};
