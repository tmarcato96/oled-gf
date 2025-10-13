#include "mainwindow.h"
#include "configwindow.h"
#include "previewtab.h"
#include "threadhelper.h"

#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
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

// Helpers
namespace {
  QString lastTwoSegments(const QString& path)
  {
    if (path.isEmpty()) return QString();

    QDir d(path);
    const QString abs = d.absolutePath();
    QDir nd(abs);

    const QString last = nd.dirName();
    if (!nd.cdUp()) return last;

    const QString parent = nd.dirName();
    return parent.isEmpty() ? last : (parent + QDir::separator() + last);
  }

  void setWorkspaceLabel(QLabel* label, const QString& path)
  {
    const QString shown = lastTwoSegments(path);
    label->setText(shown.isEmpty() ? QObject::tr("<not set>") : shown);
    label->setToolTip(path);
  }
} // namespace

MainWindow::MainWindow() :
  QMainWindow(nullptr),
  _plotStatus{0},
  _thread{nullptr}
{ // create tab and display plot
  resize(1000, 800);
  setWindowTitle("OLED-GF");
  // DEV ONLY — remove after testing
  QSettings settings("Segfault Inc.", "OLEDgf");
  settings.remove("workspaceDir");

  createMenus();
  createToolbar();
  createWorkspace();
  createCentralWidget();
  createStatusBar();
}

void MainWindow::createMenus()
{
  // menubar
  QMenuBar* menuBar = this->menuBar();

  // File menu
  QMenu* fileMenu = menuBar->addMenu(tr("&File"));

  _loadAction = new QAction("load", this);
  connect(_loadAction, &QAction::triggered, this, &MainWindow::onLoad);
  fileMenu->addAction(_loadAction);
}

void MainWindow::createToolbar()
{
  using Icon = QIcon::ThemeIcon;
  QToolBar* toolBar = addToolBar(tr("Main Toolbar"));
  toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

  _importAction = new QAction(QIcon::fromTheme(Icon::DocumentOpen), "Import", this);
  connect(_importAction, &QAction::triggered, this, &MainWindow::onLoad);
  toolBar->addAction(_importAction);

  _runAction = new QAction(QIcon::fromTheme(Icon::MediaPlaybackStart), "Run", this);
  connect(_runAction, &QAction::triggered, this, [this]() {
    if (_workspaceDir.isEmpty()) {

      const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

      const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Open Directory"), defaultDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
      if (dir.isEmpty()) return;

      _workspaceDir = dir;
      QSettings settings("Segfault Inc.", "OLEDgf");
      settings.setValue("workspaceDir", _workspaceDir);
      emit workspaceChanged(_workspaceDir);
    }

    auto layerStack = _centralStack->findChild<LayerStackWidget*>("layerStack");

    std::filesystem::path workspacePath(_workspaceDir.toStdString());
    std::filesystem::path configFilePath = workspacePath / "tmp.json";

    QStringList errors;
    if (!layerStack->makeTree(configFilePath, &errors)) {
      QMessageBox::warning(this, tr("Invalid configuration"), errors.join("\n"));
      return;
    }

    this->resetJob(QString(configFilePath.c_str()));
  });
  toolBar->addAction(_runAction);

  auto helpAction = new QAction(QIcon::fromTheme(Icon::HelpFaq), "Help", this);
  toolBar->addAction(helpAction);
}

void MainWindow::createWorkspace()
{
  QDockWidget* dock = new QDockWidget("Workspace", this);
  dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  dock->setMinimumWidth(150);
  dock->setMaximumWidth(310);

  QScrollArea* scrollArea = new QScrollArea(dock);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  QWidget* container = new QWidget(scrollArea);
  container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  auto* vbox = new QVBoxLayout(container);
  vbox->setContentsMargins(6, 6, 6, 6);
  vbox->setSpacing(8);

  auto* header = new QWidget(container);
  auto* headerLayout = new QVBoxLayout(header);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(6);
  headerLayout->setSizeConstraint(QLayout::SetFixedSize);

  auto* title = new QLabel(tr("Workspace"), header);
  title->setAlignment(Qt::AlignHCenter);

  _workspacePathLabel = new QLabel(header);
  _workspacePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  _workspacePathLabel->setAlignment(Qt::AlignHCenter);
  _workspacePathLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  auto* changeBtn = new QPushButton(tr("Change..."), header);
  connect(changeBtn, &QPushButton::clicked, this, &MainWindow::onChangeWorkspace);

  headerLayout->addWidget(title);
  headerLayout->addWidget(_workspacePathLabel);
  headerLayout->addWidget(changeBtn);
  header->setLayout(headerLayout);

  vbox->addStretch();
  vbox->addWidget(header, 0, Qt::AlignHCenter);
  vbox->addStretch();

  container->setLayout(vbox);
  scrollArea->setWidget(container);
  dock->setWidget(scrollArea);
  addDockWidget(Qt::LeftDockWidgetArea, dock);

  QSettings settings("Segfault Inc.", "OLEDgf");
  _workspaceDir = settings.value("workspaceDir").toString();

  setWorkspaceLabel(_workspacePathLabel, _workspaceDir);

  connect(this, &MainWindow::workspaceChanged, this, [this](const QString& dir) {
    setWorkspaceLabel(_workspacePathLabel, dir);
  });
}

void MainWindow::createCentralWidget()
{
  _centralStack = new QTabWidget();
  LayerStackWidget* layerStack = new LayerStackWidget;
  layerStack->setObjectName("layerStack");
  _centralStack->addTab(layerStack, "Stack Configuration");
  setCentralWidget(_centralStack);
}

void MainWindow::createStatusBar()
{
  _solverStatusLabel = new QLabel(tr("Ready"), this);
  _solverProgress = new QProgressBar(this);
  _solverProgress->setFixedWidth(140);
  _solverProgress->setTextVisible(false);
  _solverProgress->setVisible(false);

  statusBar()->addWidget(_solverStatusLabel);
  statusBar()->addPermanentWidget(_solverProgress);
}

void MainWindow::onExit() { close(); }

void MainWindow::onLoad()
{
  QSettings settings("Segfault Inc.", "OLEDgf");
  QString lastDir =
    settings.value("lastOpenDir", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

  QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), lastDir);

  if (!filePath.isEmpty()) {
    settings.setValue("lastOpenDir", QFileInfo(filePath).absolutePath());
    // open the file
    resetJob(filePath);
    _plotDiss = nullptr;
    _plotPolar = nullptr;
    _plotFit = nullptr;
    _plotMode = nullptr;
    _plotRT = nullptr;
  }
}

void MainWindow::displayResultWindow()
{
  if (_resultsList) _resultsList->clear();
  if (_centralStack->count() > 1) _centralStack->removeTab(1);

  _resultsPage = new QWidget;
  auto* hbox = new QHBoxLayout(_resultsPage);
  hbox->setContentsMargins(0, 0, 0, 0);
  hbox->setSpacing(8);

  // List of Results
  _resultsList = new QListWidget(_resultsPage);
  _resultsList->setSelectionMode(QAbstractItemView::SingleSelection);
  _resultsList->setFixedWidth(150);

  const auto mode = _thread ? _thread->worker->getMode() : Data::SolverMode::simulation;
  size_t placeholderNum;
  if (mode == Data::SolverMode::fitting) {
    _resultsList->addItem(tr("Fitting plot"));
    placeholderNum = 1;
  }
  else {
    _resultsList->addItem(tr("Dissipation plot"));
    _resultsList->addItem(tr("Substrate emission (polar)"));
    _resultsList->addItem(tr("Mode contributions"));
    _resultsList->addItem(tr("Reflectance"));
    placeholderNum = 5;
  }

  // Plot area
  _resultsPlots = new QStackedWidget(_resultsPage);
  for (size_t i = 0; i < placeholderNum; ++i) { _resultsPlots->addWidget(new QWidget); }

  hbox->addWidget(_resultsList);
  hbox->addWidget(_resultsPlots, 1);

  connect(_resultsList, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row < 0) return;
    ensurePlotCreated(row);
    _resultsPlots->setCurrentIndex(row);
  });

  _centralStack->addTab(_resultsPage, tr("Results"));
  _centralStack->setCurrentWidget(_resultsPage);
}

void MainWindow::ensurePlotCreated(int row)
{
  if (!_thread) return;

  const auto mode = _thread->worker->getMode();

  switch (mode) {
  case Data::SolverMode::fitting:
    switch (row) {
    case 0:
      if (!_plotFit) {
        _plotFit = _thread->makeFitPlot();
        _resultsPlots->removeWidget(_resultsPlots->widget(0));
        _resultsPlots->insertWidget(0, _plotFit);
      }
      break;
    }
    break;
  case Data::SolverMode::simulation:
    switch (row) {
    case 0:
      if (!_plotDiss) {
        _plotDiss = _thread->makeDissPlot();
        _resultsPlots->removeWidget(_resultsPlots->widget(0));
        _resultsPlots->insertWidget(0, _plotDiss);
      }
      break;

    case 1:
      if (!_plotPolar) {
        _plotPolar = _thread->makePolarPlot();
        _resultsPlots->removeWidget(_resultsPlots->widget(1));
        _resultsPlots->insertWidget(1, _plotPolar);
      }
      break;

    case 2:
      if (!_plotMode) {
        _plotMode = _thread->makeModePlot();
        _resultsPlots->removeWidget(_resultsPlots->widget(2));
        _resultsPlots->insertWidget(2, _plotMode);
      }
      break;

    case 3:
      if (!_plotRT) {
        _plotRT = _thread->makeRTPlot();
        _resultsPlots->removeWidget(_resultsPlots->widget(3));
        _resultsPlots->insertWidget(3, _plotRT);
      }
      break;
    }
    break;
  }
}

void MainWindow::resetJob(const QString& configFilepath)
{

  if (_thread == nullptr) {
    _thread = new UIthreading::ThreadManager(configFilepath, this);
    connect(_thread, &UIthreading::ThreadManager::errorSignal, this, [this](const QString& msg) {
      QMessageBox::critical(this, tr("Solver error"), msg);
      setUIRunning(false);
    });
    connect(_thread, &UIthreading::ThreadManager::solverStatus, this, [this](bool finished) {
      if (finished) displayResultWindow();
    });
    processWorkerSignals();
  }
  else {
    _thread->restartSolver(configFilepath);
    processWorkerSignals();
  }
}

void MainWindow::onChangeWorkspace()
{
  const QString startDir =
    _workspaceDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) : _workspaceDir;

  const QString dir = QFileDialog::getExistingDirectory(
    this, tr("Select Workspace"), startDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (dir.isEmpty()) return;

  _workspaceDir = dir;
  QSettings settings("Segfault Inc.", "OLEDgf");
  settings.setValue("workspaceDir", _workspaceDir);
  emit workspaceChanged(_workspaceDir);
}

void MainWindow::setUIRunning(bool running)
{
  if (running) {
    _solverStatusLabel->setText(tr("Running..."));
    _solverProgress->setRange(0, 0);
    _solverProgress->setVisible(true);
  }
  else {
    _solverStatusLabel->setText(tr("Ready"));
    _solverProgress->setVisible(false);
  }

  if (_runAction) _runAction->setEnabled(!running);
  if (_importAction) _importAction->setEnabled(!running);
  if (_loadAction) _loadAction->setEnabled(!running);
}

void MainWindow::processWorkerSignals()
{
  setUIRunning(true);

  static QMetaObject::Connection conn;
  if (conn) QObject::disconnect(conn);

  conn = connect(
    _thread,
    &UIthreading::ThreadManager::solverStatus,
    this,
    [this](bool finished) { setUIRunning(!finished); },
    Qt::QueuedConnection);
}