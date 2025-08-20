#include <QWidget>
#include <QComboBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

#include "configwindow.h"


LayerStackWidget::LayerStackWidget(QWidget *parent) 
    : QWidget(parent) 
    {
        QVBoxLayout *outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0,0,0,0);
        outerLayout->setSpacing(2);

        QWidget *topContainer = new QWidget;
        QFormLayout *topForm = new QFormLayout(topContainer);
        topForm->setContentsMargins(4,4,4,4);
        topForm->setSpacing(4);

        // Mode combo
        modeCombo = new QComboBox;
        modeCombo->addItems({"Fitting", "Simulation"});
        topForm->addRow("Mode:", modeCombo);

        // Mode-dependent stacked fields
        modeFields = new QStackedWidget;
        topForm->addRow(modeFields);

        // Helper for sweep fields
        auto createSweepFields = [](QWidget *parent) -> QWidget* {
            QWidget *sweepOpt = new QWidget(parent);
            QHBoxLayout *sweepLayout = new QHBoxLayout(sweepOpt);
            sweepLayout->setContentsMargins(2,2,2,2);
            sweepLayout->setSpacing(8);

            QLabel *startLabel = new QLabel("Start:");
            QLineEdit *startField = new QLineEdit;
            QLabel *stopLabel = new QLabel("Stop:");
            QLineEdit *stopField = new QLineEdit;

            sweepLayout->addWidget(startLabel);
            sweepLayout->addWidget(startField);
            sweepLayout->addWidget(stopLabel);
            sweepLayout->addWidget(stopField);

            return sweepOpt;
        };

        QWidget *fitFields = new QWidget;
        QFormLayout *fitLayout = new QFormLayout(fitFields);
        fitLayout->setContentsMargins(0,0,0,0);
        fitLayout->setSpacing(4);
        fitLayout->addRow("Sweep:", createSweepFields(fitFields));
        // File selector row (fitData)
        QWidget *fileRow = new QWidget;
        QHBoxLayout *fileLayout = new QHBoxLayout(fileRow);
        fileLayout->setContentsMargins(0,0,0,0);
        fileLayout->setSpacing(4);

        QLineEdit *fitDataEdit = new QLineEdit;
        QPushButton *browseButton = new QPushButton("Browse…");
        fileLayout->addWidget(fitDataEdit, 1);
        fileLayout->addWidget(browseButton, 0);


        fitLayout->addRow("fitData:", fileRow);
        modeFields->addWidget(fitFields);
        connect(browseButton, &QPushButton::clicked, this, [this, fitDataEdit]() {
        QString fileName = QFileDialog::getOpenFileName(
            this, tr("Select Data File"), QString(), tr("Data Files (*.txt *.csv);;All Files (*)")
            );
            if (!fileName.isEmpty()) {
                fitDataEdit->setText(fileName);
            }
        });

        QWidget *simFields = new QWidget;
        QFormLayout *simLayout = new QFormLayout(simFields);
        simLayout->setContentsMargins(0,0,0,0);
        simLayout->setSpacing(4);
        simLayout->addRow("alpha:", new QLineEdit);
        simLayout->addRow("Sweep:", createSweepFields(simFields));
        simLayout->addRow("dipole:", new QLineEdit);
        modeFields->addWidget(simFields);

        connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                modeFields, &QStackedWidget::setCurrentIndex);

        outerLayout->addWidget(topContainer, 0);

        QScrollArea *scrollArea = new QScrollArea;
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::StyledPanel);
        scrollArea->setLineWidth(2);               
        scrollArea->setContentsMargins(4, 4, 4, 4);

        container = new QWidget;
        stackLayout = new QVBoxLayout(container);
        stackLayout->setAlignment(Qt::AlignTop);
        stackLayout->setSpacing(8);
        stackLayout->setContentsMargins(4, 4, 4, 4);
        stackLayout->addStretch();
        scrollArea->setWidget(container);

        QWidget *scrollContainer = new QWidget;
        QVBoxLayout *scrollContainerLayout = new QVBoxLayout(scrollContainer);
        scrollContainerLayout->setContentsMargins(4, 4, 4, 4);
        scrollContainerLayout->setSpacing(0);
        
        QLabel *layers = new QLabel("Layers");
        layers->setAlignment(Qt::AlignCenter);
        scrollContainerLayout->addWidget(layers);
        scrollContainerLayout->addWidget(scrollArea);

        outerLayout->addWidget(scrollContainer, 1);


        QWidget *buttonContainer = new QWidget;
        QHBoxLayout *buttonLayout = new QHBoxLayout(buttonContainer);
        buttonLayout->setContentsMargins(0,0,0,0);
        buttonLayout->setSpacing(8); // space between buttons

        QPushButton *addButton = new QPushButton("+ Add Layer");
        connect(addButton, &QPushButton::clicked, this, &LayerStackWidget::addLayer);
        buttonLayout->addWidget(addButton);

        QPushButton *exportButton = new QPushButton("Export JSON");
        buttonLayout->addWidget(exportButton);

        outerLayout->addWidget(buttonContainer, 0, Qt::AlignCenter);

        addLayer();
    }

QList<QVariantMap> LayerStackWidget::getLayersData() const {
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

void LayerStackWidget::addLayer() {
    QWidget *layerWidget = new QWidget;
    layerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QFormLayout *form = new QFormLayout(layerWidget);
    form->setSpacing(4);

    QLineEdit *matEdit = new QLineEdit;
    matEdit->setObjectName("matEdit");

    QLabel *widthLab = new QLabel("width:");
    NoWheelSpinBox *widthSpin = new NoWheelSpinBox;
    widthSpin->setSuffix(" nm");
    widthSpin->setRange(0.0, 10000.0);
    widthSpin->setObjectName("widthSpin");

    QLabel *refLab = new QLabel("ref. Idx: ");
    NoWheelSpinBox *refSpin = new NoWheelSpinBox;
    refSpin->setRange(0.0, 500.0);
    refSpin->setObjectName("refSpin");
    refSpin->setFocusPolicy(Qt::ClickFocus);

    QPushButton *matButton = new QPushButton("Browse");
    connect(matButton, &QPushButton::clicked, this, [this, matEdit]() {
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Select Data File"), QString(), tr("Data Files (*.txt *.csv);;All Files (*)")
        );
        if (!fileName.isEmpty()) {
            matEdit->setText(fileName);
        }
    });
    
    QPushButton *removeButton = new QPushButton("Remove");
    connect(removeButton, &QPushButton::clicked, this, [this, layerWidget]() {
        removeLayer(layerWidget);
    });

    // Inline mat + remove button
    QWidget *matRow = new QWidget;
    QHBoxLayout *matLayout = new QHBoxLayout(matRow);
    matLayout->setContentsMargins(0, 0, 0, 0);
    matLayout->addWidget(matEdit);
    matLayout->addWidget(matButton);
    matLayout->addWidget(removeButton);

    QWidget *spinRow = new QWidget;
    QHBoxLayout *spinLayout = new QHBoxLayout(spinRow);
    spinLayout->setContentsMargins(0, 0, 0, 0);
    spinLayout->setSpacing(8);
    spinLayout->addWidget(refLab, 0);
    spinLayout->addWidget(refSpin, 1);
    spinLayout->addWidget(widthLab, 0);
    spinLayout->addWidget(widthSpin, 1);

    form->addRow("Material:", matRow);
    form->addRow(spinRow);

    // Insert before the stretch at the bottom
    stackLayout->insertWidget(stackLayout->count() - 1, layerWidget);
    layerWidgets.append(layerWidget);
}

void LayerStackWidget::removeLayer(QWidget *layerWidget) {
    layerWidgets.removeOne(layerWidget);
    stackLayout->removeWidget(layerWidget);
    layerWidget->deleteLater();
}