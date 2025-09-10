#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char* argv[])
{
  int rc = 0;
  try {
    QApplication app(argc, argv);

    MainWindow* mainWin = new MainWindow();
    mainWin->show();

    rc = app.exec();
  } catch (const std::exception& e) {
    QMessageBox::critical(nullptr, "Fatal error", e.what());
    rc = 1;
  } catch (...) {
    QMessageBox::critical(nullptr, "Fatal error", "Uknown exception");
    rc = 1;
  }
  return rc;
}