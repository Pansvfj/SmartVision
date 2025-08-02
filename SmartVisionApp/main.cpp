#include "stdafx.h"
#include "mainwindow.h"
#include <QtWidgets/QApplication>

#include <YoloDetector.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<GetModelResultType>("std::vector<std::pair<std::string,float>>");
    qRegisterMetaType<GetYoloResultType>("QPair<QImage, QStringList>");
    qRegisterMetaType<std::vector<float>>("std::vector<float>");
    qRegisterMetaType<std::vector<YoloDetection>>("std::vector<YoloDetection>");
	qRegisterMetaType<cv::Mat>("cv::Mat");

    MainWindow window;
    window.show();
    return app.exec();
}
