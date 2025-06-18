#pragma once

#include <QDockWidget>
#include <QWidget>
#include <QMainWindow>
#include <QLabel>
#include <qwt_plot.h>

class PreviewTab : public QWidget {
    Q_OBJECT

    private:
        QWidget* _targetTab;
        QLabel* _previewLab;
        QPixmap _originalPixmap;

    public slots:
        void updatePreview();

    public:
        explicit PreviewTab(QWidget* targetTab, const QString& label); //targetTab is the default parent
        explicit PreviewTab(QWidget* targetTab, const QString& label, QWidget* parent);
        QSize sizeHint() const override;
        bool hasHeightForWidth() const override;
        int heightForWidth(int width) const override;
        void resizeEvent(QResizeEvent* event) override;
        void resizePreview();
};