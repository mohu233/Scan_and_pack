#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("ProfilePower");
    a.setApplicationVersion(QStringLiteral(PROFILEPOWER_VERSION));
    a.setWindowIcon(QIcon(":/branding/profilepower.png"));
    MainWindow w;
    w.show();
    return QApplication::exec();
}
