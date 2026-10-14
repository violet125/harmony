#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QTimer>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ohos_codeblocks_prototype");
    QCoreApplication::setApplicationName("ohos_codeblocks");
    QCoreApplication::setApplicationVersion("0.1.0");
    QFile style(":/theme.qss");
    if (style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
    MainWindow window;
    window.show();
    const auto arguments = app.arguments();
    const int projectIndex = arguments.indexOf("--project");
    if (projectIndex >= 0 && projectIndex + 1 < arguments.size()) window.openProject(arguments.at(projectIndex + 1));
    if (arguments.contains("--smoke-test")) QTimer::singleShot(250, &app, &QApplication::quit);
    return app.exec();
}
