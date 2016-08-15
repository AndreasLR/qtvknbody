#include "mainwindow.hpp"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/doc/app.png"));


    MainWindow w;

    w.show();

    return a.exec();
}
