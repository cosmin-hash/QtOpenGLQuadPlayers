#pragma once
#include <QMainWindow>
#include <QStringList>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // initialDirs (optional) preload the 2x2 panes in order.
    explicit MainWindow(const QStringList& initialDirs = {}, QWidget* parent = nullptr);
    ~MainWindow() override = default;
};
