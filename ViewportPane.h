#pragma once
#include <QWidget>

class QPushButton;
class QLabel;
class GLViewport;
class MediaController;
class QComboBox;

class ViewportPane : public QWidget {
    Q_OBJECT
public:
    explicit ViewportPane(QWidget* parent = nullptr);
    ~ViewportPane() override;

    // Preload an image-sequence directory (used for startup presets / CLI args).
    void loadDirectory(const QString& dirPath);

private:
    GLViewport* m_viewport = nullptr;
    MediaController* m_controller = nullptr;

    QPushButton* m_btnLoad = nullptr;
    QPushButton* m_btnPlay = nullptr;
    QPushButton* m_btnReverse = nullptr;
    QPushButton* m_btnStop = nullptr;
    QPushButton* m_btnFF = nullptr;
    QPushButton* m_btnREW = nullptr;
    QLabel* m_status = nullptr;
    QComboBox* m_filterBox = nullptr;

    void setupUi();
    void connectSignals();
};


