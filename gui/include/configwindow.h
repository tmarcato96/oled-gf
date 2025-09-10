#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <jsonsimplecpp/node.hpp>

struct JsonSerializablePage
{
  virtual ~JsonSerializablePage() = default;

  virtual void toJson(Json::JsonNode<>::Object& root) const = 0;

  virtual bool validate(QStringList& errors) const = 0;
};

class DissipationPage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit* startEdit{};
  QLineEdit* stopEdit{};

public:
  explicit DissipationPage(QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;

  bool validate(QStringList& errors) const override;
};

class AngleSweepPage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit* startEdit{};
  QLineEdit* stopEdit{};

public:
  explicit AngleSweepPage(QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;
  bool validate(QStringList& errors) const override;
};

class SpectrumConstantPage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit* wvlEdit{};

public:
  explicit SpectrumConstantPage(QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;
  bool validate(QStringList& errors) const override;
};

class SpectrumFilePage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit* pathEdit{};

public:
  explicit SpectrumFilePage(QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;
  bool validate(QStringList& errors) const override;
};

class SpectrumGaussianPage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit *xminEdit{}, *xmaxEdit{}, *x0Edit{}, *sigmaEdit{}, *numEdit{};

public:
  explicit SpectrumGaussianPage(QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;
  bool validate(QStringList& errors) const override;
};

class DipoleConstantPage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit* dipEdit{};

public:
  explicit DipoleConstantPage(QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;
  bool validate(QStringList& errors) const override;
};

class DipoleUniformPage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit *zminEdit{}, *zmaxEdit{}, *numEdit{};

public:
  explicit DipoleUniformPage(QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;
  bool validate(QStringList& errors) const override;
};

class MaterialConstantPage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit *matnEdit{}, *matkEdit{};

public:
  explicit MaterialConstantPage(QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;
  bool validate(QStringList& errors) const override;
};

class MaterialFilePage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit* pathEdit{};

public:
  explicit MaterialFilePage(QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;
  bool validate(QStringList& errors) const override;
};

class FitFilePage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit* pathEdit{};

public:
  explicit FitFilePage(QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;
  bool validate(QStringList& errors) const override;
};

class LayerPage final
  : public QWidget
  , public JsonSerializablePage
{
  Q_OBJECT
  QLineEdit* thicknessEdit{};
  QStackedWidget* matStack;

public:
  LayerPage(QButtonGroup* emitterGroup, QWidget* parent = nullptr);

  void toJson(Json::JsonNode<>::Object& root) const override;
  bool validate(QStringList& errors) const override;

  QRadioButton* emitterCheck = nullptr;

signals:
  void removeRequested(LayerPage* self);
};

class LayerStackWidget : public QWidget
{
  Q_OBJECT
public:
  LayerStackWidget(QWidget* parent = nullptr);

  QList<QVariantMap> getLayersData() const;

  bool makeTree(const std::string& configFilePath, QStringList* outErrors);

public slots:
  void addLayer();

  void removeLayer(LayerPage* layerWidget);

private:
  QWidget* container;
  QVBoxLayout* stackLayout;
  QList<LayerPage*> layerWidgets;
  QComboBox* modeCombo;
  QStackedWidget *modeFields, *sweepStack, *spectrumStack, *dipStack;
  QTabWidget* stackTab;
  QButtonGroup* emitterGroup = nullptr;
};

class NoWheelSpinBox : public QDoubleSpinBox
{
  Q_OBJECT
public:
  explicit NoWheelSpinBox(QWidget* parent = nullptr) :
    QDoubleSpinBox(parent)
  {
    setDecimals(8);
    setFocusPolicy(Qt::ClickFocus); // only focus when clicked
  }

protected:
  void wheelEvent(QWheelEvent* event) override
  {
    // Only accept wheel events if we already have focus
    if (hasFocus()) { QDoubleSpinBox::wheelEvent(event); }
    else {
      event->ignore();
    }
  }
};
