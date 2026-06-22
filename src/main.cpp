#include <QApplication>

#include "installerwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("LFS Installer");
    QApplication::setOrganizationName("PersonalDistro");

    InstallerWindow window;
    window.show();

    return app.exec();
}
