#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    //qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    //qputenv("QT_QPA_EGLFS_WIDTH", "800");
    //qputenv("QT_QPA_EGLFS_HEIGHT", "480");
    // Approximate physical dimensions for a standard 7-inch touchscreen in millimeters:
    //qputenv("QT_QPA_EGLFS_PHYSICAL_WIDTH", "154");
    //qputenv("QT_QPA_EGLFS_PHYSICAL_HEIGHT", "85");
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
