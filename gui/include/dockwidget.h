#pragma once

#include <QDockWidget>
#include <QAction>


class LeftDockWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit LeftDockWidget(QWidget* parent = nullptr);
};