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
  connect(importAction, &QAction::triggered, this, &MainWindow::onLoad);
  toolBar->addAction(importAction);

  auto startAction = new QAction(QIcon::fromTheme(Icon::MediaPlaybackStart), "Run", this);
  connect(startAction, &QAction::triggered, this, [this]() {
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
  toolBar->addAction(startAction);

  auto helpAction = new QAction(QIcon::fromTheme(Icon::HelpFaq), "Help", this);
  toolBar->addAction(helpAction);
}

void MainWindow::createWorkspace()
{
  QDockWidget* dock = new QDockWidget("Workspace", this);
  dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  dock->setMinimumWidth(220);
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
    _centralStack->addTab(plot, "Polar Plot");
    _centralStack->setCurrentWidget(plot);
  }
}

void MainWindow::resetJob(const QString& configFilepath)
{

  if (_thread == nullptr) _thread = new UIthreading::ThreadManager(configFilepath, this);
  else _thread->worker->restartSolver(configFilepath);
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