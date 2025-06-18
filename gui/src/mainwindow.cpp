#include "mainwindow.h"
#include <previewtab.h>
#include <threadhelper.h>

#include <QEvent>
#include <QWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QTimer>
#include <QDebug>
#include <QScrollArea>
#include <QMainWindow>
#include <QwtPlotZoomer>
#include <QPen>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>

MonitoredTab::MonitoredTab(QWidget* parent) 
    : QWidget(parent),
      _previewTab{nullptr},
      _thread{nullptr},
      plot{nullptr}
      {}

MonitoredTab::MonitoredTab(QString& configFilepath, QWidget* parent) 
    : QWidget(parent),
      _previewTab{nullptr},
      _thread{new UIthreading::ThreadManager(configFilepath, this)}
      {}

void MonitoredTab::makeJob(const QString& configFilepath) {
    if (_thread == nullptr) _thread = new UIthreading::ThreadManager(configFilepath, this);
}

void MonitoredTab::setPlot(bool polarFlag) {
    plot = _thread->makePlot(polarFlag);
    plot->setParent(this);
}

void MonitoredTab::setPreviewTab(PreviewTab* tab) {
    _previewTab = tab;
    _previewTab->updatePreview();
}

void MainWindow::showPlot(QwtPlot* plot)
{
    if(plot == nullptr) {
        plot = new QwtPlot(this);
        plot->setTitle("Perfectly Accurate Plot");
        plot->setCanvasBackground(Qt::white);
        setCentralWidget(plot);

        auto curve = new QwtPlotCurve();
        curve->setTitle("Sample Curve");
        curve->setPen(Qt::blue, 2);

        // Example data points
        QVector<double> xData = {0, 1, 2, 3, 4, 5};
        QVector<double> yData = {0, 1, 4, 9, 16, 25};
        curve->setSamples(xData, yData);
        curve->attach(plot);

        auto zoomer = new QwtPlotZoomer(plot->canvas());
        zoomer->setRubberBand(QwtPlotZoomer::RectRubberBand);
        zoomer->setRubberBandPen(QPen(Qt::red));
        zoomer->setTrackerMode(QwtPlotZoomer::AlwaysOn);
    }
    _currentTab->plot = plot;
    plot->replot();
}

void MonitoredTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange && _previewTab) {
        _previewTab->updatePreview();
    }
    QWidget::changeEvent(event);
}

void MonitoredTab::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (_previewTab) {
        QTimer::singleShot(0, _previewTab, &PreviewTab::updatePreview);
    }
}

MonitoredTab* MainWindow::newBlankTab(const QString& label) {
    auto tab = new MonitoredTab(this);
    _tabList.push_back(tab);
    PreviewTab* preview = new PreviewTab(tab, label);
    tab->setPreviewTab(preview);
    _previewLayout->addWidget(preview);
    
    return tab;
}

MainWindow::MainWindow()
    : QMainWindow(nullptr)
    {   //create tab and display plot
        resize(1000, 800);   
        setWindowTitle("Perfectly Accurate Results (trust me bro)");

        createMenus();
        createToolbar();
        createPreviewTabs();
        showPlot(_currentTab->plot);
    }


MonitoredTab* MainWindow::newTabFromFile(const QString& label, const QString& configFilepath) {
    auto tab = newBlankTab(label);
    tab->makeJob(configFilepath);
    return tab;
}

void MainWindow::createMenus()
{
    //menubar
    QMenuBar *menuBar = this ->menuBar();

    // File menu
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));

    auto newTaskAction = new QAction("new job", this);
    connect(newTaskAction, &QAction::triggered, this, &MainWindow::onExit);
    fileMenu->addAction(newTaskAction);

    auto loadAction = new QAction("load", this);
    connect(loadAction, &QAction::triggered, this, &MainWindow::onExit);
    fileMenu->addAction(loadAction);

    QMenu *outfileSubMenu = fileMenu->addMenu(tr("&export results"));
    auto exportAction = new QAction("export", this);
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExit);
    outfileSubMenu->addAction(exportAction);

   //Job menu
    QMenu *jobMenu = menuBar->addMenu(tr("&Job"));

    auto startJobAction = new QAction("start job", this);
    connect(startJobAction, &QAction::triggered, this, &MainWindow::onExit);
    jobMenu->addAction(startJobAction);

    auto clearJobAction = new QAction("stop job", this);
    connect(clearJobAction, &QAction::triggered, this, &MainWindow::onExit);
    jobMenu->addAction(clearJobAction);

    auto restartJobAction = new QAction("restart job", this);
    connect(restartJobAction, &QAction::triggered, this, &MainWindow::onExit);
    jobMenu->addAction(restartJobAction);

    QMenu *setModeSubMenu = jobMenu->addMenu(tr("set job mode"));
    auto fittingJobAction = new QAction("fitting", this);
    auto simulationJobAction = new QAction("simulation", this);
    connect(fittingJobAction, &QAction::triggered, this, &MainWindow::onExit);
    connect(simulationJobAction, &QAction::triggered, this, &MainWindow::onExit);
    setModeSubMenu->addAction(fittingJobAction);
    setModeSubMenu->addAction(simulationJobAction);

    //Plot
    QMenu *plotMenu = menuBar->addMenu(tr("&Plot"));

    auto startAction = new QAction("show dissipation plot", this);
    connect(startAction, &QAction::triggered, this, &MainWindow::onExit);
    plotMenu->addAction(startAction);

    auto clearAction = new QAction("show polar plot", this);
    connect(clearAction, &QAction::triggered, this, &MainWindow::onExit);
    plotMenu->addAction(clearAction);

    QMenu *colorPlotSubMenu =  plotMenu->addMenu("color");
    auto colorAction = new QAction("line color", this);
    connect(colorAction, &QAction::triggered, this, &MainWindow::onExit);
    colorPlotSubMenu->addAction(colorAction);

    QMenu *tabPlotSubMenu = plotMenu->addMenu(tr("&table from plot"));
    auto tableAction = new QAction("CSV table", this);
    connect(tableAction, &QAction::triggered, this, &MainWindow::onExit);
    tabPlotSubMenu->addAction(tableAction);
}

void MainWindow::createToolbar()
{
    using Icon = QIcon::ThemeIcon;
    QToolBar *toolBar = addToolBar(tr("Main Toolbar"));
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    auto saveAction = new QAction(QIcon::fromTheme(Icon::DocumentSave), "Save", this);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSave);
    toolBar->addAction(saveAction);

    auto exportAction = new QAction(QIcon::fromTheme(Icon::DocumentPrint), "Export", this);
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExit);
    toolBar->addAction(exportAction);

    auto exitAction = new QAction(QIcon::fromTheme(Icon::DocumentOpen), "Import", this);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExit);
    toolBar->addAction(exitAction);
    
    auto startAction = new QAction(QIcon::fromTheme(Icon::MediaPlaybackStart), "Start", this);
    connect(startAction, &QAction::triggered, this, &MainWindow::onExit);
    toolBar->addAction(startAction);

    auto stopAction = new QAction(QIcon::fromTheme(Icon::MediaPlaybackStop), "Stop", this);
    connect(stopAction, &QAction::triggered, this, &MainWindow::onExit);
    toolBar->addAction(stopAction);

    auto helpAction = new QAction(QIcon::fromTheme(Icon::HelpFaq), "Help", this);
    connect(helpAction, &QAction::triggered, this, &MainWindow::onExit);
    toolBar->addAction(helpAction);
}


void MainWindow::createPreviewTabs() {
    QDockWidget* dock = new QDockWidget("Preview Sidebar", this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setMinimumWidth(220);
    dock->setMaximumWidth(310);

    QScrollArea* scrollArea = new QScrollArea(dock);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget* container = new QWidget(scrollArea);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    _previewLayout = new QVBoxLayout(container);
    _previewLayout->setSpacing(6);

    container->setLayout(_previewLayout);
    scrollArea->setWidget(container);
    dock->setWidget(scrollArea);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void MainWindow::onExit()
{
    close();
}

void MainWindow::onSave()
{
    qDebug() << "Save action triggered";

    // TODO: Implement saving plot image or data
}