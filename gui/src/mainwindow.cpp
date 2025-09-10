#include "mainwindow.h"
#include "configwindow.h"
#include "previewtab.h"
#include "threadhelper.h"

#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>

#include <QImageWriter>
#include <QPainter>
#include <QPdfWriter>
#include <QPen>
#include <QwtPlotZoomer>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_polar_plot.h>

MainWindow::MainWindow() :
  QMainWindow(nullptr),
  _plotStatus{0},
  _thread{nullptr}
{ // create tab and display plot
  resize(1000, 800);
  setWindowTitle("OLED-GF");

  createMenus();
  createToolbar();
  createWorkspace();
  createCentralWidget();
}

void MainWindow::createMenus()
{
  // menubar
  QMenuBar* menuBar = this->menuBar();

  // File menu
  QMenu* fileMenu = menuBar->addMenu(tr("&File"));

  auto loadAction = new QAction("load", this);
  connect(loadAction, &QAction::triggered, this, &MainWindow::onLoad);
  fileMenu->addAction(loadAction);

  // Job menu
  QMenu* jobMenu = menuBar->addMenu(tr("&Job"));

  auto newBlankJobAction = new QAction("new blank job", this);
  jobMenu->addAction(newBlankJobAction);

  auto newJobAction = new QAction("new job", this);
  jobMenu->addAction(newJobAction);

  auto restartJobAction = new QAction("restart job", this);
  jobMenu->addAction(restartJobAction);

  // Plot
  QMenu* plotMenu = menuBar->addMenu(tr("&Plot"));

  auto fitPlotAction = new QAction("Fitting plot", this);
  connect(fitPlotAction, &QAction::triggered, this, [this]() { displayPlot(Data::SolverMode::fitting); });
  plotMenu->addAction(fitPlotAction);

  auto plotDisAction = new QAction("Dissipation plot", this);
  connect(plotDisAction, &QAction::triggered, this, [this]() { displayPlot(Data::SolverMode::simulation); });
  plotMenu->addAction(plotDisAction);

  auto plotPolarAction = new QAction("Polar plot", this);
  connect(plotPolarAction, &QAction::triggered, this, [this]() { displayPolarPlot(); });
  plotMenu->addAction(plotPolarAction);
}

void MainWindow::createToolbar()
{
  using Icon = QIcon::ThemeIcon;
  QToolBar* toolBar = addToolBar(tr("Main Toolbar"));
  toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

  auto importAction = new QAction(QIcon::fromTheme(Icon::DocumentOpen), "Import", this);
  toolBar->addAction(importAction);

  auto startAction = new QAction(QIcon::fromTheme(Icon::MediaPlaybackStart), "Start Plot", this);
  connect(startAction, &QAction::triggered, this, [this]() {
    auto dir = QFileDialog::getExistingDirectory(
      this, tr("Open Directory"), {}, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (dir.isEmpty()) return;

    auto layerStack = _centralStack->findChild<LayerStackWidget*>("layerStack");

    std::filesystem::path workspacePath(dir.toStdString());
    std::filesystem::path configFilePath = workspacePath / "tmp.json";

    QStringList errors;
    if (!layerStack->makeTree(configFilePath, &errors)) {
      QMessageBox::warning(this, tr("Invalid configuration"), errors.join("\n"));
      return;
    }

    this->resetJob(QString(configFilePath.c_str()));
  });
  toolBar->addAction(startAction);

  auto helpAction = new QAction(QIcon::fromTheme(Icon::HelpFaq), "Help", this);
  toolBar->addAction(helpAction);
}

void MainWindow::createWorkspace()
{
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

void MainWindow::createCentralWidget()
{
  _centralStack = new QTabWidget();
  LayerStackWidget* layerStack = new LayerStackWidget;
  layerStack->setObjectName("layerStack");
  _centralStack->addTab(layerStack, "Stack Configuration");
  setCentralWidget(_centralStack);
}

void MainWindow::onExit() { close(); }

void MainWindow::onLoad()
{
  QSettings settings("SegFault Inc.", "OLEDgf");
  QString lastDir =
    settings.value("lastOpenDir", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

  QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), lastDir);

  if (!filePath.isEmpty()) {
    settings.setValue("lastOpenDir", QFileInfo(filePath).absolutePath());
    // open the file
    resetJob(filePath);
  }
}

void MainWindow::displayPlot(Data::SolverMode calledMode)
{
  if (_thread == nullptr) QMessageBox::warning(this, tr("missing job"), tr("Please start a job first!"));
  else if (_thread->worker->getMode() != calledMode)
    QMessageBox::warning(this, tr("job mode mismatch"), tr("The plot type selected does not match the job mode!"));
  else {
    auto plot = _thread->makePlot(false);
    QString plotLabel = "Plot";
    switch (calledMode) {
    case Data::SolverMode::fitting: plotLabel.prepend("Fit "); break;
    case Data::SolverMode::simulation: plotLabel.prepend("Dissipation "); break;
    }
    if (_centralStack->count() > 1) _centralStack->removeTab(1);
    _centralStack->addTab(plot, plotLabel);
    _centralStack->setCurrentWidget(plot);
  }
}

void MainWindow::displayPolarPlot()
{
  if (_thread == nullptr) QMessageBox::warning(this, tr("missing job"), tr("Please start a job first!"));
  else {
    auto plot = _thread->makePlot(true);
    _centralStack->addTab(plot, "Polar Plot");
    _centralStack->setCurrentWidget(plot);
  }
}

void MainWindow::resetJob(const QString& configFilepath)
{

  if (_thread == nullptr) _thread = new UIthreading::ThreadManager(configFilepath, this);
  else _thread->worker->restartSolver(configFilepath);
}