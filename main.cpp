#include <QApplication>
#include <QStringList>
#include "MainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // Any directory arguments preload the panes (row-major: top-left first).
    QStringList dirs;
    for (int i = 1; i < argc; ++i) dirs << QString::fromLocal8Bit(argv[i]);

    MainWindow w(dirs);
    w.resize(1400, 900);
    w.show();
    return app.exec();
}
