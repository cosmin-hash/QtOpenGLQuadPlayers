#include <QApplication>
#include <QSurfaceFormat>
#include <QStringList>
#include "MainWindow.h"

int main(int argc, char** argv) {
    // Request an OpenGL 4.1 core context — the highest version macOS supports,
    // and ample for the textured-quad filter pipeline. Setting the default
    // format before any QOpenGLWidget is created is required on macOS to get a
    // core profile (it otherwise yields a legacy 2.1 context).
    QSurfaceFormat fmt;
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setVersion(4, 1);
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);

    // Any directory arguments preload the panes (row-major: top-left first).
    QStringList dirs;
    for (int i = 1; i < argc; ++i) dirs << QString::fromLocal8Bit(argv[i]);

    MainWindow w(dirs);
    w.resize(1400, 900);
    w.show();
    return app.exec();
}
