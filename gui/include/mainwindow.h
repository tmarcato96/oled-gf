#pragma once

#include <QMainWindow>
#include <QAction>
#include <QEvent>
#include <QList>
#include <QVBoxLayout>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <QwtPlotZoomer>

#include <threadhelper.h>
#include <previewtab.h>

class MonitoredTab : public QWidget
{
    Q_OBJECT
    
    PreviewTab* _previewTab; //keeps previewtab reference in raw pointer
    UIthreading::ThreadManager* _thread;

    public:
        MonitoredTab() = delete; //helps avoid memory leaks
        MonitoredTab(QWidget* parent = nullptr);
        MonitoredTab(QString& configFilepath, QWidget* parent = nullptr);
            
        void makeJob(const QString& configFilepath);
        void setPreviewTab(PreviewTab* tab);
        void setPlot(bool polarFlag);

        QwtPlot *plot;

    protected:
        void changeEvent(QEvent* event) override;
        void showEvent(QShowEvent* event) override;
};

class MainWindow : public QMainWindow //true main window
{
protected:
    Q_OBJECT

    void createMenus();
    void createToolbar();
    void createPreviewTabs();
    void showPlot(QwtPlot* plot);

    QList<MonitoredTab*> _tabList;
    MonitoredTab* _currentTab;
    QVBoxLayout* _previewLayout;

public:
    MainWindow();
    MonitoredTab* newBlankTab(const QString& label="");
    MonitoredTab* newTabFromFile(const QString& configFilepath, const QString& label="");

protected slots:
    void onExit();
    void onSave();
};