#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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

void LayerStackWidget::makeTree()
{
  using JsonNode = Json::JsonNode<>;
  using JsonObject = JsonNode::Object;

  std::unique_ptr<JsonObject> rootObject(new JsonObject());

  const int page = modeCombo->currentIndex();
  if (page == 1) {
    // Simulation
    if (auto* p = dynamic_cast<JsonSerializablePage*>(sweepStack->currentWidget())) { p->toJson(*rootObject); }
    double alpha = stackTab->widget(1)->findChild<QLineEdit*>("alphaEdit")->text().toDouble();
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

  std::filesystem::path rootPath = PROJECT_ROOT;
  std::filesystem::path dataPath = rootPath / "examples/data/test.json";
  std::ofstream os(dataPath);

  root.print(os);
}

DissipationPage::DissipationPage(QWidget* parent) :
  QWidget(parent)
{
  auto* l = new QHBoxLayout(this);
  startEdit = new QLineEdit;
  startEdit->setObjectName("dissStartEdit");
  stopEdit = new QLineEdit;
  stopEdit->setObjectName("dissStopEdit");

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

AngleSweepPage::AngleSweepPage(QWidget* parent) :
  QWidget(parent)
{
  auto* l = new QHBoxLayout(this);
  startEdit = new QLineEdit;
  startEdit->setObjectName("angleStartEdit");
  stopEdit = new QLineEdit;
  stopEdit->setObjectName("angleStopEdit");

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

SpectrumConstantPage::SpectrumConstantPage(QWidget* parent) :
  QWidget(parent)
{
  auto* l = new QHBoxLayout(this);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(8);

  wvlEdit = new QLineEdit;
  wvlEdit->setObjectName("wvlConstant");
  wvlEdit->setValidator(new QDoubleValidator(0.0, 1e9, 6, wvlEdit));

  l->addWidget(new QLabel("Wavelength (nm)"));
  l->addWidget(wvlEdit);
}

void SpectrumConstantPage::toJson(Json::JsonNode<>::Object& root) const
{

  using JsonNode = Json::JsonNode<>;

  root["spectrum"] = std::make_unique<JsonNode>(wvlEdit->text().toDouble());
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

SpectrumGaussianPage::SpectrumGaussianPage(QWidget* parent) :
  QWidget(parent)
{
  auto* l = new QFormLayout(this);
  l->setContentsMargins(0, 0, 0, 0);

  xminEdit = new QLineEdit;
  xmaxEdit = new QLineEdit;
  x0Edit = new QLineEdit;
  sigmaEdit = new QLineEdit;
  numEdit = new QLineEdit;

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

DipoleConstantPage::DipoleConstantPage(QWidget* parent) :
  QWidget(parent)
{

  auto* l = new QHBoxLayout(this);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(4);

  dipEdit = new QLineEdit;
  l->addWidget(dipEdit);
}

void DipoleConstantPage::toJson(Json::JsonNode<>::Object& root) const
{

  using JsonNode = Json::JsonNode<>;

  root["dipole"] = std::make_unique<JsonNode>(dipEdit->text().toDouble());
}

DipoleUniformPage::DipoleUniformPage(QWidget* parent) :
  QWidget(parent)
{
  auto* l = new QFormLayout(this);
  l->setContentsMargins(0, 0, 0, 0);

  zminEdit = new QLineEdit;
  zmaxEdit = new QLineEdit;
  numEdit = new QLineEdit;

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

MaterialConstantPage::MaterialConstantPage(QWidget* parent) :
  QWidget(parent)
{

  auto* l = new QHBoxLayout(this);
  matnEdit = new QLineEdit;
  matkEdit = new QLineEdit;

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