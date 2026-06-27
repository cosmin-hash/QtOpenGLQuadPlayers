#include "MainWindow.h"
#include <QGridLayout>
#include <QWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QKeySequence>
#include "ViewportPane.h"

MainWindow::MainWindow(const QStringList& initialDirs, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Qt OpenGL Quad Players");
    setMinimumSize(1200, 900);

    // Create menu bar
    auto *file_menu = menuBar()->addMenu("&File");
    file_menu->addAction("&Exit", QKeySequence::Quit, this, &QWidget::close);

    auto *view_menu = menuBar()->addMenu("&View");
    view_menu->addAction("&Full Screen", QKeySequence::FullScreen, this, &QWidget::showFullScreen);
    view_menu->addAction("&Normal", this, &QWidget::showNormal);

    auto *central = new QWidget(this);
    auto *grid = new QGridLayout(central);
    grid->setContentsMargins(10, 10, 10, 10);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    auto *p00 = new ViewportPane(this);
    auto *p01 = new ViewportPane(this);
    auto *p10 = new ViewportPane(this);
    auto *p11 = new ViewportPane(this);

    grid->addWidget(p00, 0, 0);
    grid->addWidget(p01, 0, 1);
    grid->addWidget(p10, 1, 0);
    grid->addWidget(p11, 1, 1);

    // Optionally preload sequences into the panes (in row-major order).
    ViewportPane* panes[4] = {p00, p01, p10, p11};
    for (int i = 0; i < initialDirs.size() && i < 4; ++i) {
        panes[i]->loadDirectory(initialDirs[i]);
    }

    setCentralWidget(central);

    // Create status bar
    statusBar()->showMessage("Ready");
    statusBar()->addPermanentWidget(new QLabel("Players: 4"));
}
