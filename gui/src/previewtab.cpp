#include "previewtab.h"
#include <QLabel>
#include <QApplication>
#include <QVBoxLayout>
#include <QPixmap>
#include <QTimer>
#include <QMainWindow>

PreviewTab::PreviewTab(QWidget* targetTab, const QString& label, QWidget* parent)
    : QWidget(parent), 
      _targetTab(targetTab)
    {
        QVBoxLayout* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);

        // wraps content in child widget to ensure top-alignment
        QWidget* contentWidget = new QWidget(this);
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(0, 0, 0, 0);

        QLabel* nameLabel = new QLabel(label, this);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("font-weight: bold; padding-bottom: 4px;");
        contentLayout->addWidget(nameLabel);

        _previewLab = new QLabel("No Preview");
        _previewLab->setAlignment(Qt::AlignCenter);
        _previewLab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        _previewLab->setScaledContents(false);
        contentLayout->addWidget(_previewLab);

        contentWidget->setLayout(contentLayout);
        outerLayout->addWidget(contentWidget, 0, Qt::AlignTop);  //forces top-alignment
        setLayout(outerLayout);

        QTimer::singleShot(0, this, SLOT(updatePreview()));
    }

PreviewTab::PreviewTab(QWidget* targetTab, const QString& label) {
    PreviewTab(targetTab, label, targetTab);
}

void PreviewTab::updatePreview() {
    if (!_targetTab || !_targetTab->isVisible()) {
        _previewLab->setText("No target tab visible");
        return;
    }

    if (_targetTab->size().isEmpty()) {
        _previewLab->setText("Central widget has zero size");
        return;
    }

    QPixmap pixmap = _targetTab->grab();

    if (pixmap.isNull()) {
        _previewLab->setText("Failed to capture pixmap");
        return;
    }

    _originalPixmap = pixmap; // save original unscaled pixmap
    resizePreview();  // Refactor scaling logic
}

bool PreviewTab::hasHeightForWidth() const {
    return true;
}

int PreviewTab::heightForWidth(int width) const {
    if (!_originalPixmap.isNull()) {
        QSize pixSize = _originalPixmap.size();
        return (pixSize.height() * width) / pixSize.width();
    }
    return width * 3 / 4;  // fallback 4:3 ratio
}

QSize PreviewTab::sizeHint() const {
    if (_previewLab && !_previewLab->pixmap().isNull()) {
        return _previewLab->pixmap().size();
    }
    return QWidget::sizeHint();
}

void PreviewTab::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    resizePreview();  // Respond to container size change
}

void PreviewTab::resizePreview() {
    if (!_originalPixmap.isNull()) {
        int w = width();
        w = qMin(w, _originalPixmap.width());

        int h = heightForWidth(w);
        QPixmap scaled = _originalPixmap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        _previewLab->setPixmap(scaled);
        _previewLab->setFixedSize(scaled.size());
        updateGeometry();
    }
}