#pragma once

#include <QDockWidget>
#include <QWidget>
#include <QMainWindow>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>

class PreviewTab : public QWidget {
    Q_OBJECT

    private:
        QWidget* _targetTab;
        QLabel* _previewLab;
        QPixmap _originalPixmap;

    public slots:
        void updatePreview();
    
    protected:
        void mousePressEvent(QMouseEvent* event) override;

    public:
        explicit PreviewTab(QWidget* targetTab, const QString& label=""); //targetTab is the default parent
        explicit PreviewTab(QWidget* targetTab, const QString& label, QWidget* parent);

        QSize sizeHint() const override;
        bool hasHeightForWidth() const override;
        int heightForWidth(int width) const override;
        void resizeEvent(QResizeEvent* event) override;
        void resizePreview();

        void resetTargetTab(QWidget* targetTab, const QString& label="");
    
    signals:
        void clicked();

};