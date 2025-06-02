#include "dockwidget.h"
#include <QTextEdit> 

LeftDockWidget::LeftDockWidget(QWidget* parent)
    : QDockWidget("Left Panel", parent) {

    auto* textEdit = new QTextEdit(this);
    setWidget(textEdit);

    // Optional settings
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
}