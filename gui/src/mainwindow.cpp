#include "mainwindow.h"
#include <previewtab.h>
#include <threadhelper.h>

#include <QString>
#include <QEvent>
#include <QWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QTimer>
#include <QScrollArea>
#include <QMainWindow>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QProcess>

#include <QwtPlotZoomer>
#include <QPen>
#include <QPainter>
#include <QPdfWriter>
#include <QImageWriter>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_polar_plot.h>



//monitored window stuff
void MonitoredTab::makeCanvas() {
    auto emptyPlot = new QwtPlot(this);
    emptyPlot->setTitle("Perfectly Accurate Plot");
    emptyPlot->setCanvasBackground(Qt::white);

    auto zoomer = new QwtPlotZoomer(emptyPlot->canvas());
    zoomer->setRubberBand(QwtPlotZoomer::RectRubberBand);
    zoomer->setRubberBandPen(QPen(Qt::red));
    zoomer->setTrackerMode(QwtPlotZoomer::AlwaysOn);

    plot = emptyPlot;
    if(_layout==nullptr) {
        _layout = new QVBoxLayout(this);
        _layout->setContentsMargins(0, 0, 0, 0);
        setLayout(_layout);
    }
    else{
        while (QLayoutItem* item = _layout->takeAt(0)) {
            if (QWidget* widget = item->widget()) {
                widget->setParent(nullptr);
                widget->deleteLater(); 
            }
        }
    }
    _layout->addWidget(plot);
    _plotAvail = 0;
}

MonitoredTab::MonitoredTab(QWidget* parent) 
    : QWidget(parent),
      _previewTab{nullptr},
      _thread{nullptr},
      _layout{nullptr}
      {makeCanvas();}

MonitoredTab::MonitoredTab(QString& configFilepath, QWidget* parent) 
    : QWidget(parent),
      _previewTab{nullptr},
      _thread{new UIthreading::ThreadManager(configFilepath, this)},
      _layout{nullptr}
      {makeCanvas();}

//some functionality wrappers
void MonitoredTab::makeJob(const QString& configFilepath) {
    if (_thread == nullptr) _thread = new UIthreading::ThreadManager(configFilepath, this);
    else{QMessageBox::warning(this, tr("solver alive"), tr("job already exists, use reset job instead"));}
}

void MonitoredTab::resetJob(const QString& configFilepath) {
    if(_thread == nullptr) _thread = new UIthreading::ThreadManager(configFilepath, this);
    else{_thread->worker->restartSolver(configFilepath);}
}

void MonitoredTab::resetJob() {
    if(_thread == nullptr) return;
    _thread->worker->restartSolver();
}

bool MonitoredTab::plotAvail() {
    return _plotAvail;
}

void MonitoredTab::setPlot(bool polarFlag) {
    if (_thread == nullptr) return;                     //maybe displaying sth would be nice
    while (QLayoutItem* item = _layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->setParent(nullptr);
            widget->deleteLater();
        }
    }
    plot = _thread->makePlot(polarFlag);
    _layout->addWidget(plot);

    _plotAvail = 1;
}

void MonitoredTab::setPreviewTab(PreviewTab* tab) {
    _previewTab = tab;
    _previewTab->updatePreview();
}

void MonitoredTab::saveToFile(const QString& savePath) {
    if (_thread == nullptr) return; //maybe displaying sth would be nice
    _thread->worker->exportResults(savePath);
}


//mainwindow stuff
void MainWindow::refreshPreviewTab(MonitoredTab* tab) {
    
    for(const auto saveTab : _tabList) {
        if(saveTab != tab) _tabList.push_back(tab);
    }

    _previewLayout->removeWidget(tab->_previewTab);
    PreviewTab* preview = new PreviewTab(tab->plot);
    tab->setPreviewTab(preview);
    connect(preview, &PreviewTab::clicked, this, [this, tab]() {
        _centralStack->setCurrentWidget(tab);
        _currentTab = tab;
    });
    _previewLayout->addWidget(preview);
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

void MainWindow::newCurrentBlankTab(const QString& label) {

    auto* newTab = new MonitoredTab(this);
    _centralStack->addWidget(newTab);

    PreviewTab* preview = new PreviewTab(newTab->plot, label, this);
    newTab->setPreviewTab(preview);

    connect(preview, &PreviewTab::clicked, this, [this, newTab]() {
        _centralStack->setCurrentWidget(newTab);
        _currentTab = newTab;
    });

    _previewLayout->addWidget(preview);
    _currentTab = newTab;
    _centralStack->setCurrentWidget(newTab);
    _tabList.push_back(newTab);
}

MainWindow::MainWindow()
    : QMainWindow(nullptr),
      _plotStatus{0}
    {   //create tab and display plot
        resize(1000, 800);   
        setWindowTitle("Perfectly Accurate Results (trust me bro)");

        createMenus();
        createToolbar();      
        createPreviewTabs(); //takes care of _previewLayout
        createCentralWidget();

        newCurrentBlankTab();
        createCanvas();

    }

void MainWindow::newCurrentTabFromFile(const QString& configFilepath, const QString& label) {
    newCurrentBlankTab(label);
    _currentTab->makeJob(configFilepath);
}

void MainWindow::createMenus() {
    //menubar
    QMenuBar *menuBar = this->menuBar();

    // File menu
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));

    auto loadAction = new QAction("load", this);
    connect(loadAction, &QAction::triggered, this, &MainWindow::onLoad);
    fileMenu->addAction(loadAction);

    QMenu *outfileSubMenu = fileMenu->addMenu(tr("&export results"));
    auto exportAction = new QAction("export", this);
    connect(exportAction, &QAction::triggered, this, &MainWindow::onSave);
    outfileSubMenu->addAction(exportAction);

   //Job menu
    QMenu *jobMenu = menuBar->addMenu(tr("&Job"));

    auto newBlankJobAction = new QAction("new blank job", this);
    connect(newBlankJobAction, &QAction::triggered, this, &MainWindow::onNewTab);
    jobMenu->addAction(newBlankJobAction);

    auto newJobAction = new QAction("new job", this);
    connect(newJobAction, &QAction::triggered, this, &MainWindow::onOpen);
    jobMenu->addAction(newJobAction);


    auto restartJobAction = new QAction("restart job", this);
    connect(restartJobAction, &QAction::triggered, this, &MainWindow::onReload);
    jobMenu->addAction(restartJobAction);

    //Plot
    QMenu *plotMenu = menuBar->addMenu(tr("&Plot"));

    auto fitPlotAction = new QAction("show fitting plot", this);
    connect(fitPlotAction, &QAction::triggered, this,[this](){
                                                        displayPlot(Data::SolverMode::fitting);});
    plotMenu->addAction(fitPlotAction);

    auto plotDisAction = new QAction("show dissipation plot", this);
    connect(plotDisAction, &QAction::triggered, this, [this](){
                                                        displayPlot(Data::SolverMode::simulation);});
    plotMenu->addAction(plotDisAction);

    auto plotPolarAction = new QAction("show polar plot", this);
    connect(plotPolarAction, &QAction::triggered, this, displayPolarPlot);
    plotMenu->addAction(plotPolarAction);

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

    auto saveAction = new QAction(QIcon::fromTheme(Icon::DocumentSave), "Save plot", this);
    connect(saveAction, &QAction::triggered, this, &MainWindow::savePlot);
    toolBar->addAction(saveAction);

    auto exportAction = new QAction(QIcon::fromTheme(Icon::DocumentPrint), "Export", this);
    connect(exportAction, &QAction::triggered, this, &MainWindow::onSave);
    toolBar->addAction(exportAction);

    auto importAction = new QAction(QIcon::fromTheme(Icon::DocumentOpen), "Import", this);
    connect(importAction, &QAction::triggered, this, &MainWindow::onOpen);
    toolBar->addAction(importAction);
    
    auto startAction = new QAction(QIcon::fromTheme(Icon::MediaPlaybackStart), "Start Plot", this);
    connect(startAction, &QAction::triggered, this, [this](){if (this->_currentTab->_thread != nullptr) {
                                                             auto solverMode = this->_currentTab->_thread->worker->getMode();
                                                             displayPlot(solverMode);}});
    toolBar->addAction(startAction);

    auto stopAction = new QAction(QIcon::fromTheme(Icon::MediaPlaybackStop), "Stop Plot", this);
    connect(stopAction, &QAction::triggered, this, &MainWindow::deletePlot);
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

void MainWindow::createCentralWidget() {
    _centralStack = new QStackedWidget(this);
    setCentralWidget(_centralStack);
}

void MainWindow::createCanvas() {
    if(_currentTab->plot == nullptr) {
        auto failurePlot = new QwtPlot(this);
        failurePlot = new QwtPlot(this);
        failurePlot->setTitle("Backup Plot (tab failure)");
        failurePlot->setCanvasBackground(Qt::white);

        auto curve = new QwtPlotCurve();
        curve->setTitle("Sample Curve");
        curve->setPen(Qt::blue, 2);

        // Example data points
        QVector<double> xData = {0, 1, 2, 3, 4, 5};
        QVector<double> yData = {0, 1, 4, 9, 16, 25};
        curve->setSamples(xData, yData);
        curve->attach(failurePlot);

        auto zoomer = new QwtPlotZoomer(failurePlot->canvas());
        zoomer->setRubberBand(QwtPlotZoomer::RectRubberBand);
        zoomer->setRubberBandPen(QPen(Qt::red));
        zoomer->setTrackerMode(QwtPlotZoomer::AlwaysOn);
        _plotStatus = 0;
    }
    else {
        if(auto plot = dynamic_cast<QwtPlot*> (_currentTab->plot)){
            plot->replot(); //updates plot
            _plotStatus = 1;
        }
        else if (auto plot = dynamic_cast<QwtPolarPlot*> (_currentTab->plot))
            plot->replot();
            _plotStatus = 1;
    }
        _centralStack->setCurrentWidget(_currentTab);
}

void MainWindow::onExit() {
    close();
}

void MainWindow::onOpen() {
    QSettings settings("SegFault Inc.", "OLEDgf");
    QString lastDir = settings.value("lastOpenDir", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), lastDir);

    if (!filePath.isEmpty()) {
        settings.setValue("lastOpenDir", QFileInfo(filePath).absolutePath());
        // open the file
        if(_plotStatus) newCurrentTabFromFile(filePath);
        else { _currentTab->resetJob(filePath);}
    }
}

void MainWindow::onNewTab() {
    newCurrentBlankTab();
}

void MainWindow::onLoad() {
    QSettings settings("SegFault Inc.", "OLEDgf");
    QString lastDir = settings.value("lastOpenDir", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), lastDir);

    if (!filePath.isEmpty()) {
        settings.setValue("lastOpenDir", QFileInfo(filePath).absolutePath());
        // open the file
        _currentTab->resetJob(filePath);
    }
}

void MainWindow::onReload() {
    _currentTab->resetJob();
}

void MainWindow::onSave() {
    QSettings settings("SegFault Inc.", "OLEDgf");
    QString filePath = QFileDialog::getSaveFileName(
            this,                     
            tr("Save File"),
            QDir::homePath(),                  
            tr("Text Files (*.txt);;All Files (*)")
        );

    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        settings.setValue("lastSaveDir", fileInfo.absolutePath());
        _currentTab->saveToFile(filePath);
    }
}

void MainWindow::savePlot()
{
    auto plot = _currentTab->plot;
    if (!plot) {
        QMessageBox::warning(this, tr("Save Plot"), tr("No plot to save."));
        return;
    }

    QSettings settings("YourCompany", "YourApp");
    QString lastDir = settings.value("lastSavePlotDir", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

    QString filter = tr("PNG Image (*.png);;JPEG Image (*.jpg);;PDF File (*.pdf)");
    QString selectedFilter;
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Plot As"),
        lastDir + "/plot.png",
        filter,
        &selectedFilter
    );

    if (filePath.isEmpty()) return;

    settings.setValue("lastSavePlotDir", QFileInfo(filePath).absolutePath());

    if (selectedFilter.contains("*.png")) {
        QImage image(plot->size(), QImage::Format_ARGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        plot->render(&painter);
        image.save(filePath, "PNG");
    } 
    else if (selectedFilter.contains("*.jpg")) {
        QImage image(plot->size(), QImage::Format_RGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        plot->render(&painter);
        image.save(filePath, "JPG");
    }
     else if (selectedFilter.contains("*.pdf")) {
        QPdfWriter pdf(filePath);
        pdf.setPageSize(QPageSize(QSizeF(plot->width(), plot->height()), QPageSize::Point));
        pdf.setResolution(800);

        QPainter painter(&pdf);
        plot->render(&painter);
        painter.end();
    } 
    else {
        QMessageBox::warning(this, tr("Unsupported Format"), tr("The selected file format is not supported."));
    }
}

void MainWindow::displayPlot(Data::SolverMode calledMode) {
    if(_currentTab->_thread == nullptr) QMessageBox::warning(this, tr("missing job"), tr("Please start a job first!"));
    else if (_currentTab->_thread->worker->getMode() != calledMode) QMessageBox::warning(this, tr("job mode mismatch"), tr("The plot type selected does not match the job mode!"));
    else {
        _currentTab->setPlot(0);
        refreshPreviewTab(_currentTab);
    }
}

void MainWindow::displayPolarPlot() {
    if(_currentTab->_thread == nullptr) QMessageBox::warning(this, tr("missing job"), tr("Please start a job first!"));
    else {
        _currentTab->setPlot(1);
        refreshPreviewTab(_currentTab);
    }
}

void MainWindow::deletePlot() {
    _currentTab->makeCanvas();
    refreshPreviewTab(_currentTab);
    _plotStatus = 0;
}