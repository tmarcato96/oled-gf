#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

#include "configwindow.h"


LayerStackWidget::LayerStackWidget(QWidget *parent) : QWidget(parent) {
    QVBoxLayout *outerLayout = new QVBoxLayout(this);

    // Scroll area for the stack
    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    outerLayout->addWidget(scrollArea);

    // Container for all layers (inside scroll area)
    container = new QWidget;
    stackLayout = new QVBoxLayout(container);
    stackLayout->setSpacing(8);
    stackLayout->setContentsMargins(10, 10, 10, 10); 
    stackLayout->addStretch();
    scrollArea->setWidget(container);

    // Add button goes OUTSIDE scroll area
    QPushButton *addButton = new QPushButton("+ Add Layer");
    connect(addButton, &QPushButton::clicked, this, &LayerStackWidget::addLayer);
    outerLayout->addWidget(addButton, 0, Qt::AlignCenter);

    addLayer(); // adds first layer
}


QList<QVariantMap> LayerStackWidget::getLayersData() const {
    QList<QVariantMap> data;
    for (auto layerWidget : layerWidgets) {
        auto nameEdit = layerWidget->findChild<QLineEdit*>("nameEdit");
        auto thicknessSpin = layerWidget->findChild<NoWheelSpinBox*>("thicknessSpin");
        auto refSpin = layerWidget->findChild<NoWheelSpinBox*>("refSpin");

        QVariantMap layer;
        layer["name"] = nameEdit->text();
        layer["thickness"] = thicknessSpin->value();
        layer["n"] = refSpin->value();
        data.append(layer);
    }
    return data;
}

void LayerStackWidget::addLayer() {
    QWidget *layerWidget = new QWidget;
    layerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QFormLayout *form = new QFormLayout(layerWidget);
    form->setSpacing(6);

    QLineEdit *nameEdit = new QLineEdit;
    nameEdit->setObjectName("nameEdit");

    NoWheelSpinBox *thicknessSpin = new NoWheelSpinBox;
    thicknessSpin->setSuffix(" nm");
    thicknessSpin->setRange(0.0, 10000.0);
    thicknessSpin->setObjectName("thicknessSpin");

    NoWheelSpinBox *refSpin = new NoWheelSpinBox;
    refSpin->setRange(0.0, 50.0);
    refSpin->setObjectName("refSpin");
    refSpin->setFocusPolicy(Qt::ClickFocus);

    QPushButton *removeButton = new QPushButton("Remove");
    connect(removeButton, &QPushButton::clicked, this, [this, layerWidget]() {
        removeLayer(layerWidget);
    });

    // Inline name + remove button
    QWidget *nameRow = new QWidget;
    QHBoxLayout *nameLayout = new QHBoxLayout(nameRow);
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->addWidget(nameEdit);
    nameLayout->addWidget(removeButton);

    form->addRow("Material:", nameRow);
    form->addRow("Thickness:", thicknessSpin);
    form->addRow("Ref Idx:", refSpin);

    // Insert before the stretch at the bottom
    stackLayout->insertWidget(stackLayout->count() - 1, layerWidget);

    layerWidgets.append(layerWidget);
}

void LayerStackWidget::removeLayer(QWidget *layerWidget) {
    layerWidgets.removeOne(layerWidget);
    stackLayout->removeWidget(layerWidget);
    layerWidget->deleteLater();
}