#pragma once

#include <QDockWidget>
#include <QWidget>
#include <QMainWindow>
#include <QLabel>

class PreviewTab : public QWidget {
    Q_OBJECT

    private:
        QMainWindow* _targetWindow;
        QLabel* _previewLab;
        QPixmap _originalPixmap;
    
    public slots:
        void updatePreview();

    public:
        explicit PreviewTab(QMainWindow* targetWindow, const QString& label, QWidget* parent = nullptr);
        QSize sizeHint() const override;
        bool hasHeightForWidth() const override;
        int heightForWidth(int width) const override;
        void resizeEvent(QResizeEvent* event) override;
        void resizePreview();
};