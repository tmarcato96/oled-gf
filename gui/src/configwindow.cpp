#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "configwindow.h"
#include <jsonsimplecpp/node.hpp>

#ifndef PROJECT_ROOT
    #define PROJECT_ROOT "./"
#endif

// Helpers
namespace {
  inline void adjustStackHeight(QStackedWidget* stack)
  {
    if (!stack) return;

    if (QWidget* page = stack->currentWidget()) {
      const int h = page->sizeHint().height();
      stack->setMinimumHeight(h);
      stack->setMaximumHeight(h);
    }
  }

  template<class Sender, class Signal>
  inline void bindStackHeightTo(Sender* sender, Signal signal, QStackedWidget* stack)
  {
    QObject::connect(sender, signal, stack, [stack](auto&&... args) {
      Q_UNUSED(sizeof...(args));
      adjustStackHeight(stack);
    });
    QObject::connect(stack, &QStackedWidget::currentChanged, stack, [stack](int) { adjustStackHeight(stack); });

    adjustStackHeight(stack);
  }

  void setError(QWidget* w, bool on) { w->setStyleSheet(on ? "border: 1px solid red;" : ""); }
} // namespace

LayerStackWidget::LayerStackWidget(QWidget* parent) :
  QWidget(parent)
{
  emitterGroup = new QButtonGroup(this);
  emitterGroup->setExclusive(true);

  QVBoxLayout* outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);
  outerLayout->setSpacing(2);

  QWidget* topContainer = new QWidget;
  QFormLayout* topForm = new QFormLayout(topContainer);
  topForm->setContentsMargins(4, 4, 4, 4);
  topForm->setSpacing(4);

  // Mode combo
  modeCombo = new QComboBox;
  modeCombo->addItems({"Fitting", "Simulation"});
  topForm->addRow("Mode:", modeCombo);

  // Mode-dependent stacked fields
  modeFields = new QStackedWidget;
  topForm->addRow(modeFields);

  modeFields->addWidget(new FitFilePage);

  QWidget* simFields = new QWidget;
  QFormLayout* simLayout = new QFormLayout(simFields);
  simLayout->setContentsMargins(0, 0, 0, 0);
  simLayout->setSpacing(4);

  auto* simTypeCombo = new QComboBox;
  simTypeCombo->addItems({"Mode Dissipation", "Angle Sweep"});
  simTypeCombo->setObjectName("simTypeCombo");
  simLayout->addRow("Type", simTypeCombo);

  sweepStack = new QStackedWidget;

  sweepStack->addWidget(new DissipationPage(simFields));

  sweepStack->addWidget(new AngleSweepPage(simFields));

  connect(
    simTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), sweepStack, &QStackedWidget::setCurrentIndex);

  simLayout->addRow(sweepStack);

  modeFields->addWidget(simFields);

  connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), modeFields, &QStackedWidget::setCurrentIndex);

  outerLayout->addWidget(topContainer, 0);

  stackTab = new QTabWidget;

  // Widget for material stack
  container = new QWidget;
  stackLayout = new QVBoxLayout(container);
  stackLayout->setAlignment(Qt::AlignTop);
  stackLayout->setSpacing(8);
  stackLayout->setContentsMargins(4, 4, 4, 4);
  stackLayout->addStretch();

  auto* layersPage = new QWidget;
  auto* layersPageLayout = new QVBoxLayout(layersPage);
  layersPageLayout->setContentsMargins(0, 0, 0, 0);
  layersPageLayout->setSpacing(0);

  auto* layersScroll = new QScrollArea(layersPage);
  layersScroll->setWidgetResizable(true);
  layersScroll->setFrameShape(QFrame::StyledPanel); // optional, for a border
  layersScroll->setLineWidth(1);
  layersScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  layersScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  layersScroll->setWidget(container);
  layersPageLayout->addWidget(layersScroll);

  stackTab->addTab(layersPage, "Layers");

  // Widget for spectrum
  QWidget* spectrumWidget = new QWidget;
  auto* spectrumForm = new QFormLayout(spectrumWidget);

  auto* wvlCombo = new QComboBox;
  wvlCombo->addItems({"Constant", "File", "Gaussian"});
  wvlCombo->setObjectName("wvlCombo");
  spectrumForm->addRow("Spectrum", wvlCombo);

  spectrumStack = new QStackedWidget;
  // Page 0: Constant wavelength
  spectrumStack->addWidget(new SpectrumConstantPage(spectrumWidget));
  // Page 1: File spectrum
  spectrumStack->addWidget(new SpectrumFilePage(spectrumWidget));
  // Page 2: Gaussian Spectrum
  spectrumStack->addWidget(new SpectrumGaussianPage(spectrumWidget));

  connect(
    wvlCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), spectrumStack, &QStackedWidget::setCurrentIndex);

  bindStackHeightTo(wvlCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), spectrumStack);

  spectrumStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  spectrumStack->setContentsMargins(0, 0, 0, 0);

  spectrumForm->addRow(spectrumStack);

  // Dipole position
  auto* dipCombo = new QComboBox;
  dipCombo->addItems({"Constant", "Uniform"});
  spectrumForm->addRow("Emitter position", dipCombo);

  dipStack = new QStackedWidget;
  // Page 0: Single Dipole Position
  dipStack->addWidget(new DipoleConstantPage(spectrumWidget));
  // Page 1: Uniform distribution
  dipStack->addWidget(new DipoleUniformPage(spectrumWidget));

  connect(dipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), dipStack, &QStackedWidget::setCurrentIndex);

  bindStackHeightTo(dipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), dipStack);

  dipStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  dipStack->setContentsMargins(0, 0, 0, 0);

  spectrumForm->addRow(dipStack);

  // Dipole orientation
  auto* alphaEdit = new QLineEdit;
  alphaEdit->setObjectName("alphaEdit");
  spectrumForm->addRow("Dipole Orientation", alphaEdit);

  stackTab->addTab(spectrumWidget, "Emitter");

  outerLayout->addWidget(stackTab, 1);

  QPushButton* addButton = new QPushButton("+ Add Layer");
  connect(addButton, &QPushButton::clicked, this, &LayerStackWidget::addLayer);

  outerLayout->addWidget(addButton, 0, Qt::AlignCenter);

  addLayer();
}

QList<QVariantMap> LayerStackWidget::getLayersData() const
{
  QList<QVariantMap> data;
  for (auto layerWidget : layerWidgets) {
    auto matEdit = layerWidget->findChild<QLineEdit*>("matEdit");
    auto widthSpin = layerWidget->findChild<NoWheelSpinBox*>("widthSpin");
    auto refSpin = layerWidget->findChild<NoWheelSpinBox*>("refSpin");

    QVariantMap layer;
    layer["mat"] = matEdit->text();
    layer["width"] = widthSpin->value();
    layer["n"] = refSpin->value();
    data.append(layer);
  }
  return data;
}

void LayerStackWidget::addLayer()
{
  auto* layerWidget = new LayerPage(emitterGroup, container);
  connect(layerWidget, &LayerPage::removeRequested, this, &LayerStackWidget::removeLayer);

  // Insert before the stretch at the bottom
  stackLayout->insertWidget(stackLayout->count() - 1, layerWidget);
  layerWidgets.append(layerWidget);
}

void LayerStackWidget::removeLayer(LayerPage* layerWidget)
{
  if (!layerWidget) return;

  if (layerWidget->emitterCheck) emitterGroup->removeButton(layerWidget->emitterCheck);

  layerWidgets.removeOne(layerWidget);
  stackLayout->removeWidget(layerWidget);
  layerWidget->deleteLater();
}

bool LayerStackWidget::makeTree(const std::string& configFilePath, QStringList* outErrors)
{
  QStringList errors;

  // Validate first
  if (modeCombo->currentIndex() == 1) {
    if (auto* p = dynamic_cast<JsonSerializablePage*>(sweepStack->currentWidget())) { p->validate(errors); }
  }
  else {
    if (auto* p = dynamic_cast<JsonSerializablePage*>(modeFields->currentWidget())) p->validate(errors);
  }

  if (auto* p = dynamic_cast<JsonSerializablePage*>(spectrumStack->currentWidget())) { p->validate(errors); }

  if (auto* p = dynamic_cast<JsonSerializablePage*>(dipStack->currentWidget())) { p->validate(errors); }

  double alpha;
  {
    bool ok = false;
    alpha = stackTab->widget(1)->findChild<QLineEdit*>("alphaEdit")->text().toDouble(&ok);
    if (!ok) errors << "Emitter: dipole orientation is not a number";
  }

  for (auto layer : layerWidgets) {
    auto* lPtr = dynamic_cast<JsonSerializablePage*>(layer);
    if (lPtr) lPtr->validate(errors);
  }

  if (!emitterGroup->checkedButton()) { errors << "At least one layer must be marked as Emitter!"; }

  if (!errors.isEmpty()) {
    if (outErrors) *outErrors = errors;
    return false;
  }

  using JsonNode = Json::JsonNode<>;
  using JsonObject = JsonNode::Object;

  std::unique_ptr<JsonObject> rootObject(new JsonObject());

  const int page = modeCombo->currentIndex();
  if (page == 1) {
    // Simulation
    if (auto* p = dynamic_cast<JsonSerializablePage*>(sweepStack->currentWidget())) { p->toJson(*rootObject); }
    (*rootObject)["alpha"] = std::make_unique<JsonNode>(alpha);
  }
  else if (page == 0) {
    // Fitting
    if (auto* p = dynamic_cast<JsonSerializablePage*>(modeFields->currentWidget())) { p->toJson(*rootObject); }
  }

  // Emitter Characteristics
  if (auto* p = dynamic_cast<JsonSerializablePage*>(spectrumStack->currentWidget())) { p->toJson(*rootObject); }

  if (auto* p = dynamic_cast<JsonSerializablePage*>(dipStack->currentWidget())) { p->toJson(*rootObject); }

  for (qsizetype i = 0; i < layerWidgets.size(); ++i) {
    std::string layerKey = "layer" + std::to_string(i + 1);
    auto layerObj = std::make_unique<JsonObject>();
    layerWidgets[i]->toJson(*layerObj);
    (*rootObject)[layerKey] = std::make_unique<JsonNode>(std::move(layerObj));
  }

  // Print
  JsonNode root;
  root.value = std::move(rootObject);

  std::ofstream cfgOs(configFilePath);
  if (!cfgOs) {
    if (outErrors) *outErrors << "Failed to open config file for writing.";
    return false;
  }
  root.print(cfgOs);
  return true;
}

DissipationPage::DissipationPage(QWidget* parent) :
  QWidget(parent)
{
  auto* l = new QHBoxLayout(this);
  startEdit = new QLineEdit;
  startEdit->setObjectName("dissStartEdit");
  startEdit->setValidator(new QDoubleValidator(0, 4, 2));

  stopEdit = new QLineEdit;
  stopEdit->setObjectName("dissStopEdit");
  stopEdit->setValidator(new QDoubleValidator(0, 4, 2));

  l->addWidget(new QLabel("In-plane wavevector (norm)"));
  l->addWidget(startEdit);
  l->addWidget(stopEdit);
}

void DissipationPage::toJson(Json::JsonNode<>::Object& root) const
{
  using JsonNode = Json::JsonNode<>;
  using JsonObject = JsonNode::Object;

  auto simTypeNode = std::make_unique<JsonNode>("modeDissipation");
  root["simtype"] = std::move(simTypeNode);

  auto sweepObj = std::make_unique<JsonObject>();
  (*sweepObj)["start"] = std::make_unique<JsonNode>(startEdit->text().toDouble());
  (*sweepObj)["stop"] = std::make_unique<JsonNode>(stopEdit->text().toDouble());
  root["sweep"] = std::make_unique<JsonNode>(std::move(sweepObj));
}

bool DissipationPage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(startEdit, hasError);
  setError(stopEdit, hasError);

  bool ok1 = false, ok2 = false;
  const double start = startEdit->text().toDouble(&ok1);
  const double stop = stopEdit->text().toDouble(&ok2);

  if (!ok1) {
    errors << "Dissipation: start is not a number.";
    hasError = true;
    setError(startEdit, hasError);
  }
  if (!ok2) {
    errors << "Dissipation: stop is not a number.";
    hasError = true;
    setError(stopEdit, hasError);
  }
  if (!hasError && start >= stop) {
    errors << "Dissipation: start must be less than stop.";
    hasError = true;
    setError(startEdit, hasError);
    setError(stopEdit, hasError);
  }
  return !hasError;
}

AngleSweepPage::AngleSweepPage(QWidget* parent) :
  QWidget(parent)
{
  auto* l = new QHBoxLayout(this);
  startEdit = new QLineEdit;
  startEdit->setObjectName("angleStartEdit");
  startEdit->setValidator(new QDoubleValidator(0, 90, 2));

  stopEdit = new QLineEdit;
  stopEdit->setObjectName("angleStopEdit");
  stopEdit->setValidator(new QDoubleValidator(0, 90, 2));

  l->addWidget(new QLabel("Angle"));
  l->addWidget(startEdit);
  l->addWidget(stopEdit);
}

void AngleSweepPage::toJson(Json::JsonNode<>::Object& root) const
{
  using JsonNode = Json::JsonNode<>;
  using JsonObject = JsonNode::Object;

  auto simTypeNode = std::make_unique<JsonNode>("angleSweep");
  root["simtype"] = std::move(simTypeNode);

  auto sweepObj = std::make_unique<JsonObject>();
  (*sweepObj)["start"] = std::make_unique<JsonNode>(startEdit->text().toDouble());
  (*sweepObj)["stop"] = std::make_unique<JsonNode>(stopEdit->text().toDouble());
  root["sweep"] = std::make_unique<JsonNode>(std::move(sweepObj));
}

bool AngleSweepPage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(startEdit, hasError);
  setError(stopEdit, hasError);

  bool ok1 = false, ok2 = false;
  const double start = startEdit->text().toDouble(&ok1);
  const double stop = stopEdit->text().toDouble(&ok2);

  if (!ok1) {
    errors << "AngleSweep: start is not a number.";
    hasError = true;
    setError(startEdit, hasError);
  }
  if (!ok2) {
    errors << "AngleSweep: stop is not a number.";
    hasError = true;
    setError(stopEdit, hasError);
  }
  if (!hasError && start >= stop) {
    errors << "AngleSweep: start must be less than stop.";
    hasError = true;
    setError(startEdit, hasError);
    setError(stopEdit, hasError);
  }

  return !hasError;
}

SpectrumConstantPage::SpectrumConstantPage(QWidget* parent) :
  QWidget(parent)
{
  auto* l = new QHBoxLayout(this);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(8);

  wvlEdit = new QLineEdit;
  wvlEdit->setObjectName("wvlConstant");
  wvlEdit->setValidator(new QDoubleValidator(200, 5000, 2, wvlEdit));

  l->addWidget(new QLabel("Wavelength (nm)"));
  l->addWidget(wvlEdit);
}

void SpectrumConstantPage::toJson(Json::JsonNode<>::Object& root) const
{

  using JsonNode = Json::JsonNode<>;

  root["spectrum"] = std::make_unique<JsonNode>(wvlEdit->text().toDouble());
}

bool SpectrumConstantPage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(wvlEdit, hasError);

  bool ok = false;
  wvlEdit->text().toDouble(&ok);

  if (!ok) {
    errors << "Constant Spectrum: wavelength is not a number.";
    hasError = true;
    setError(wvlEdit, hasError);
  }

  return !hasError;
}

SpectrumFilePage::SpectrumFilePage(QWidget* parent) :
  QWidget(parent)
{

  auto* l = new QHBoxLayout(this);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(8);

  pathEdit = new QLineEdit;
  auto* fileButton = new QPushButton("Browse");

  l->addWidget(pathEdit, 1);
  l->addWidget(fileButton, 0);
  connect(fileButton, &QPushButton::clicked, this, [this]() {
    const auto file =
      QFileDialog::getOpenFileName(this, tr("Select Data File"), {}, tr("Data Files (*.txt *.csv);;All Files (*)"));
    if (!file.isEmpty()) pathEdit->setText(file);
  });
}

void SpectrumFilePage::toJson(Json::JsonNode<>::Object& root) const
{
  using JsonNode = Json::JsonNode<>;

  root["spectrum"] = std::make_unique<JsonNode>(pathEdit->text().toStdString());
}

bool SpectrumFilePage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(pathEdit, hasError);

  if (pathEdit->text().isEmpty()) {
    errors << "Spectrum: file missing.";
    hasError = true;
    setError(pathEdit, hasError);
  }

  return !hasError;
}

SpectrumGaussianPage::SpectrumGaussianPage(QWidget* parent) :
  QWidget(parent)
{
  auto* l = new QFormLayout(this);
  l->setContentsMargins(0, 0, 0, 0);

  xminEdit = new QLineEdit;
  xminEdit->setValidator(new QDoubleValidator(200, 5000, 2));
  xmaxEdit = new QLineEdit;
  xmaxEdit->setValidator(new QDoubleValidator(200, 5000, 2));
  x0Edit = new QLineEdit;
  x0Edit->setValidator(new QDoubleValidator(200, 5000, 2));
  sigmaEdit = new QLineEdit;
  sigmaEdit->setValidator(new QDoubleValidator(1e-5, 5000, 2));
  numEdit = new QLineEdit;
  numEdit->setValidator(new QDoubleValidator(1, 200, 0));

  l->addRow("Min", xminEdit);
  l->addRow("Max", xmaxEdit);
  l->addRow("Center", x0Edit);
  l->addRow("Sigma", sigmaEdit);
  l->addRow("N", numEdit);
}

void SpectrumGaussianPage::toJson(Json::JsonNode<>::Object& root) const
{

  using JsonNode = Json::JsonNode<>;
  using JsonObject = JsonNode::Object;

  auto specObject = std::make_unique<JsonObject>();
  (*specObject)["xmin"] = std::make_unique<JsonNode>(xminEdit->text().toDouble());
  (*specObject)["xmax"] = std::make_unique<JsonNode>(xmaxEdit->text().toDouble());
  (*specObject)["x0"] = std::make_unique<JsonNode>(x0Edit->text().toDouble());
  (*specObject)["sigma"] = std::make_unique<JsonNode>(sigmaEdit->text().toDouble());
  (*specObject)["N"] = std::make_unique<JsonNode>(numEdit->text().toDouble());
  auto distObject = std::make_unique<JsonObject>();
  (*distObject)["gaussian"] = std::make_unique<JsonNode>(std::move(specObject));
  root["spectrum"] = std::make_unique<JsonNode>(std::move(distObject));
}

bool SpectrumGaussianPage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(xminEdit, hasError);
  setError(xmaxEdit, hasError);
  setError(x0Edit, hasError);
  setError(sigmaEdit, hasError);
  setError(numEdit, hasError);

  bool ok1 = false, ok2 = false, ok3 = false, ok4 = false, ok5 = false;
  const double xmin = xminEdit->text().toDouble(&ok1);
  const double xmax = xmaxEdit->text().toDouble(&ok2);
  const double x0 = x0Edit->text().toDouble(&ok3);
  sigmaEdit->text().toDouble(&ok4);
  numEdit->text().toDouble(&ok5);

  if (!ok1) {
    errors << "Gaussian Spectrum: xmin is not a number.";
    hasError = true;
    setError(xminEdit, hasError);
  }
  if (!ok2) {
    errors << "Gaussian Spectrum: xmax is not a number.";
    hasError = true;
    setError(xmaxEdit, hasError);
  }
  if (!ok3) {
    errors << "Gaussian Spectrum: x0 is not a number.";
    hasError = true;
    setError(x0Edit, hasError);
  }
  if (!ok4) {
    errors << "Gaussian Spectrum: sigma is not a number.";
    hasError = true;
    setError(sigmaEdit, hasError);
  }
  if (!ok5) {
    errors << "Gaussian Spectrum: N is not a number.";
    hasError = true;
    setError(numEdit, hasError);
  }
  if (!hasError && xmin >= xmax) {
    errors << "Gaussian Spectrum: xmin must be less than xmax.";
    hasError = true;
    setError(xminEdit, hasError);
    setError(xmaxEdit, hasError);
  }
  if (!hasError && (x0 <= xmin || x0 >= xmax)) {
    errors << "Gaussian Spectrum: x0 must be between xmin and xmax";
    hasError = true;
    setError(xminEdit, hasError);
    setError(xmaxEdit, hasError);
    setError(x0Edit, hasError);
  }
  return !hasError;
}

DipoleConstantPage::DipoleConstantPage(QWidget* parent) :
  QWidget(parent)
{

  auto* l = new QHBoxLayout(this);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(4);

  dipEdit = new QLineEdit;
  dipEdit->setValidator(new QDoubleValidator(0, 1, 4));
  l->addWidget(dipEdit);
}

void DipoleConstantPage::toJson(Json::JsonNode<>::Object& root) const
{

  using JsonNode = Json::JsonNode<>;

  root["dipole"] = std::make_unique<JsonNode>(dipEdit->text().toDouble());
}

bool DipoleConstantPage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(dipEdit, hasError);

  bool ok = false;
  dipEdit->text().toDouble(&ok);

  if (!ok) {
    errors << "Emitter: dipole position  is not a number.";
    hasError = true;
    setError(dipEdit, hasError);
  }
  return !hasError;
}

DipoleUniformPage::DipoleUniformPage(QWidget* parent) :
  QWidget(parent)
{
  auto* l = new QFormLayout(this);
  l->setContentsMargins(0, 0, 0, 0);

  zminEdit = new QLineEdit;
  zminEdit->setValidator(new QDoubleValidator(0, 1, 4));
  zmaxEdit = new QLineEdit;
  zmaxEdit->setValidator(new QDoubleValidator(0, 1, 4));
  numEdit = new QLineEdit;
  numEdit->setValidator(new QDoubleValidator(1, 50, 0));

  l->addRow("Min", zminEdit);
  l->addRow("Max", zmaxEdit);
  l->addRow("N", numEdit);
}

void DipoleUniformPage::toJson(Json::JsonNode<>::Object& root) const
{

  using JsonNode = Json::JsonNode<>;
  using JsonObject = JsonNode::Object;

  auto dipObject = std::make_unique<JsonObject>();
  (*dipObject)["zmin"] = std::make_unique<JsonNode>(zminEdit->text().toDouble());
  (*dipObject)["zmax"] = std::make_unique<JsonNode>(zmaxEdit->text().toDouble());
  (*dipObject)["N"] = std::make_unique<JsonNode>(numEdit->text().toDouble());
  auto distObject = std::make_unique<JsonObject>();
  (*distObject)["uniform"] = std::make_unique<JsonNode>(std::move(dipObject));
  root["dipole"] = std::make_unique<JsonNode>(std::move(distObject));
}

bool DipoleUniformPage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(zminEdit, hasError);
  setError(zmaxEdit, hasError);
  setError(numEdit, hasError);

  bool ok1 = false, ok2 = false, ok3 = false;
  const double zmin = zminEdit->text().toDouble(&ok1);
  const double zmax = zmaxEdit->text().toDouble(&ok2);
  numEdit->text().toDouble(&ok3);

  if (!ok1) {
    errors << "Emitter: dipole zmin is not a number.";
    hasError = true;
    setError(zminEdit, hasError);
  }
  if (!ok2) {
    errors << "Emitter: dipole zmax is not a number.";
    hasError = true;
    setError(zmaxEdit, hasError);
  }
  if (!ok3) {
    errors << "Emitter: dipole N is not a number.";
    hasError = true;
    setError(numEdit, hasError);
  }
  if (!hasError && zmin >= zmax) {
    errors << "Emitter: dipole zmin must be less than zmax.";
    hasError = true;
    setError(zminEdit, hasError);
    setError(zmaxEdit, hasError);
  }
  return !hasError;
}

MaterialConstantPage::MaterialConstantPage(QWidget* parent) :
  QWidget(parent)
{

  auto* l = new QHBoxLayout(this);
  matnEdit = new QLineEdit;
  matnEdit->setValidator(new QDoubleValidator(0, 100, 2));
  matkEdit = new QLineEdit;
  matkEdit->setValidator(new QDoubleValidator(0, 100, 6));

  l->addWidget(new QLabel("n"));
  l->addWidget(matnEdit);
  l->addWidget(new QLabel("k"));
  l->addWidget(matkEdit);
}

void MaterialConstantPage::toJson(Json::JsonNode<>::Object& root) const
{

  using JsonNode = Json::JsonNode<>;
  using JsonList = JsonNode::List;

  auto matList = std::make_unique<JsonList>();
  matList->push_back(std::make_unique<JsonNode>(matnEdit->text().toDouble()));
  matList->push_back(std::make_unique<JsonNode>(matkEdit->text().toDouble()));
  root["material"] = std::make_unique<JsonNode>(std::move(matList));
}

bool MaterialConstantPage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(matnEdit, hasError);
  setError(matkEdit, hasError);
  bool ok1 = false, ok2 = false;
  matnEdit->text().toDouble(&ok1);
  matkEdit->text().toDouble(&ok2);

  if (!ok1) {
    errors << "Material: n is not a number.";
    hasError = true;
    setError(matnEdit, hasError);
  }
  if (!ok2) {
    errors << "Material: k is not a number.";
    hasError = true;
    setError(matkEdit, hasError);
  }

  return !hasError;
}

MaterialFilePage::MaterialFilePage(QWidget* parent) :
  QWidget(parent)
{

  auto* l = new QHBoxLayout(this);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(8);

  pathEdit = new QLineEdit;
  auto* fileButton = new QPushButton("Browse");

  l->addWidget(pathEdit, 1);
  l->addWidget(fileButton, 0);
  connect(fileButton, &QPushButton::clicked, this, [this]() {
    const auto file =
      QFileDialog::getOpenFileName(this, tr("Select Data File"), {}, tr("Data Files (*.txt *.csv);;All Files (*)"));
    if (!file.isEmpty()) pathEdit->setText(file);
  });
}

void MaterialFilePage::toJson(Json::JsonNode<>::Object& root) const
{
  using JsonNode = Json::JsonNode<>;

  root["material"] = std::make_unique<JsonNode>(pathEdit->text().toStdString());
}

bool MaterialFilePage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(pathEdit, hasError);

  if (pathEdit->text().isEmpty()) {
    errors << "Material: file missing.";
    hasError = true;
    setError(pathEdit, hasError);
  }
  return !hasError;
}

FitFilePage::FitFilePage(QWidget* parent) :
  QWidget(parent)
{
  auto* f = new QFormLayout(this);
  f->setContentsMargins(0, 0, 0, 0);
  f->setSpacing(4);

  auto* fitField = new QWidget;
  auto* l = new QHBoxLayout(fitField);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(8);

  pathEdit = new QLineEdit;
  auto* fileButton = new QPushButton("Browse");

  l->addWidget(pathEdit, 1);
  l->addWidget(fileButton, 0);
  connect(fileButton, &QPushButton::clicked, this, [this]() {
    const auto file =
      QFileDialog::getOpenFileName(this, tr("Select Data File"), {}, tr("Data Files (*.txt *.csv);;All Files (*)"));
    if (!file.isEmpty()) pathEdit->setText(file);
  });

  f->addRow("Fitting Data", fitField);
}

void FitFilePage::toJson(Json::JsonNode<>::Object& root) const
{
  using JsonNode = Json::JsonNode<>;

  root["fitData"] = std::make_unique<JsonNode>(pathEdit->text().toStdString());
}

bool FitFilePage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(pathEdit, hasError);

  if (pathEdit->text().isEmpty()) {
    errors << "Fitting: file missing.";
    hasError = true;
    setError(pathEdit, hasError);
  }
  return !hasError;
}

LayerPage::LayerPage(QButtonGroup* emitterGroup, QWidget* parent) :
  QWidget(parent)
{
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  auto* f = new QFormLayout(this);
  f->setSpacing(4);

  // Materials
  auto* matCombo = new QComboBox;
  matCombo->addItems({"Constant", "File"});

  emitterCheck = new QRadioButton("Emitter");
  emitterCheck->setObjectName("emitterCheck");

  QWidget* matModeRow = new QWidget;
  auto* matModeLayout = new QHBoxLayout(matModeRow);
  matModeLayout->setContentsMargins(0, 0, 0, 0);
  matModeLayout->setSpacing(8);
  matModeLayout->addWidget(matCombo);
  matModeLayout->addWidget(emitterCheck);

  f->addRow("Material", matModeRow);
  if (emitterGroup) emitterGroup->addButton(emitterCheck);

  matStack = new QStackedWidget;

  matStack->addWidget(new MaterialConstantPage(this));
  matStack->addWidget(new MaterialFilePage(this));

  connect(matCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), matStack, &QStackedWidget::setCurrentIndex);

  f->addRow(matStack);

  thicknessEdit = new QLineEdit;
  thicknessEdit->setValidator(new QDoubleValidator(-1, 1, 4));
  f->addRow("Thickness (m)", thicknessEdit);

  // Remove button
  auto* removeButton = new QPushButton("Remove");
  connect(removeButton, &QPushButton::clicked, this, [this] { emit removeRequested(this); });
  f->setWidget(f->rowCount(), QFormLayout::LabelRole, removeButton);
}

void LayerPage::toJson(Json::JsonNode<>::Object& root) const
{
  using JsonNode = Json::JsonNode<>;

  root["thickness"] = std::make_unique<JsonNode>(thicknessEdit->text().toDouble());
  double emitterFlag = emitterCheck->isChecked() ? 1 : 0;
  root["emitter"] = std::make_unique<JsonNode>(emitterFlag);

  if (auto* p = dynamic_cast<JsonSerializablePage*>(matStack->currentWidget())) { p->toJson(root); }
}

bool LayerPage::validate(QStringList& errors) const
{
  bool hasError = false;
  setError(thicknessEdit, hasError);

  bool ok = false;
  thicknessEdit->text().toDouble(&ok);

  if (!ok) {
    errors << "Layer: thickness is not a number.";
    hasError = true;
    setError(thicknessEdit, hasError);
  }

  if (auto* p = dynamic_cast<JsonSerializablePage*>(matStack->currentWidget())) hasError = !(p->validate(errors));

  return !hasError;
}