#include <QWidget>
#include <QComboBox>
#include <QStackedWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QWheelEvent>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

class LayerStackWidget : public QWidget {
    Q_OBJECT
public:
    LayerStackWidget(QWidget *parent = nullptr);

    QList<QVariantMap> getLayersData() const;

public slots:
    void addLayer();

    void removeLayer(QWidget *layerWidget);

private:
    QWidget *container;
    QVBoxLayout *stackLayout;
    QList<QWidget*> layerWidgets;
    QComboBox *modeCombo;
    QStackedWidget *modeFields;
};

class NoWheelSpinBox : public QDoubleSpinBox {
    Q_OBJECT
public:
    explicit NoWheelSpinBox(QWidget *parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        setDecimals(8);
        setFocusPolicy(Qt::ClickFocus); // only focus when clicked
    }

protected:
    void wheelEvent(QWheelEvent *event) override {
        // Only accept wheel events if we already have focus
        if (hasFocus()) {
            QDoubleSpinBox::wheelEvent(event);
        } else {
            event->ignore();
        }
    }
};