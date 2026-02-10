#include "ui/mainwindow.h"
#include <QApplication>
#include <QDir>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CCS Charger");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("CCS Charger Project");

    // Set working directory so DBC file can be found relative to executable
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    ccs::MainWindow window;
    window.show();

    return app.exec();
}
