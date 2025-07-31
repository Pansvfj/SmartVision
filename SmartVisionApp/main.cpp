#include "stdafx.h"
#include "mainwindow.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<GetModelResultType>("std::vector<std::pair<std::string,float>>");
    qRegisterMetaType<GetYoloResultType>("QPair<QImage, QStringList>");

    MainWindow window;
    window.show();
    return app.exec();
}
