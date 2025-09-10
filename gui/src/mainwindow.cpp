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

  // Plot
  QMenu* plotMenu = menuBar->addMenu(tr("&Plot"));

  _fitPlotAction = new QAction("Fitting plot", this);
  connect(_fitPlotAction, &QAction::triggered, this, [this]() { displayPlot(Data::SolverMode::fitting); });
  plotMenu->addAction(_fitPlotAction);

  _dissPlotAction = new QAction("Dissipation plot", this);
  connect(_dissPlotAction, &QAction::triggered, this, [this]() { displayPlot(Data::SolverMode::simulation); });
  plotMenu->addAction(_dissPlotAction);

  _polarPlotAction = new QAction("Polar plot", this);
  connect(_polarPlotAction, &QAction::triggered, this, [this]() { displayPolarPlot(); });
  plotMenu->addAction(_polarPlotAction);
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
    if (_centralStack->count() > 1) _centralStack->removeTab(1);
    _centralStack->addTab(plot, "Polar Plot");
    _centralStack->setCurrentWidget(plot);
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
    processWorkerSignals();
  }
  else {
    _thread->worker->restartSolver(configFilepath);
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
  if (_fitPlotAction) _fitPlotAction->setEnabled(!running);
  if (_dissPlotAction) _dissPlotAction->setEnabled(!running);
  if (_polarPlotAction) _polarPlotAction->setEnabled(!running);
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