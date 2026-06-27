#include "ViewportPane.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QComboBox>
#include "GLViewport.h"
#include "MediaController.h"
#include "MediaIcons.h"

namespace {
// Flat dark transport bar: rounded icon buttons with hover/pressed feedback and
// a green accent on the primary Play action.
const char* kTransportQss = R"(
#transportBar {
    background: #1e1f22;
    border-radius: 8px;
}
#transportBar QPushButton {
    background: #2b2d31;
    border: none;
    border-radius: 6px;
}
#transportBar QPushButton:hover  { background: #3a3d42; }
#transportBar QPushButton:pressed { background: #4a4d52; }
#transportBar QPushButton#playBtn         { background: #2f8f4e; }
#transportBar QPushButton#playBtn:hover   { background: #38a85b; }
#transportBar QPushButton#playBtn:pressed { background: #2a7d45; }
#transportBar QComboBox {
    background: #2b2d31;
    color: #e6e7e9;
    border: 1px solid #3a3d42;
    border-radius: 6px;
    padding: 3px 10px;
    min-height: 28px;
}
#transportBar QComboBox:hover { border-color: #5a5d63; }
#transportBar QComboBox::drop-down { border: none; width: 18px; }
#transportBar QComboBox QAbstractItemView {
    background: #2b2d31;
    color: #e6e7e9;
    border: 1px solid #3a3d42;
    selection-background-color: #3a6df0;
    outline: none;
}
#transportBar QLabel {
    color: #aeb0b4;
    font-size: 12px;
}
)";

// Apply the shared look to a transport push button.
void styleButton(QPushButton* b, const QIcon& icon, const QString& tip) {
    b->setIcon(icon);
    b->setIconSize(QSize(22, 22));
    b->setText(QString());
    b->setFixedSize(40, 34);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
}
} // namespace

ViewportPane::ViewportPane(QWidget* parent)
    : QWidget(parent)
{
    m_viewport = new GLViewport(this);
    m_controller = new MediaController(this);

    setupUi();
    connectSignals();

    // The viewport pulls decoded frames from the worker's queue when notified.
    m_viewport->setFrameQueue(m_controller->frameQueue());
    connect(m_controller, &MediaController::frameAvailable,
            m_viewport, &GLViewport::consumeFrames);

    connect(m_controller, &MediaController::statusText,
            m_status, &QLabel::setText);

    // Live metrics from the decode worker, refreshed once per second.
    connect(m_controller, &MediaController::metricsUpdated, this,
            [this](const DecodeWorker::Metrics& m) {
        const QString dir = m.direction > 0 ? QStringLiteral("▶")
                          : m.direction < 0 ? QStringLiteral("◀")
                                            : QStringLiteral("■");
        m_status->setText(
            QStringLiteral("%1 %2 fps | dec %3 ms | q %4/%5 | drop %6 | %7/%8 | %9x")
                .arg(dir)
                .arg(m.fps, 0, 'f', 1)
                .arg(m.avgDecodeMs, 0, 'f', 1)
                .arg(m.queueDepth).arg(m.queueCapacity)
                .arg(m.dropped)
                .arg(m.total ? m.index + 1 : 0).arg(m.total)
                .arg(m.speed, 0, 'f', 2));
    });
}

ViewportPane::~ViewportPane() = default;

void ViewportPane::loadDirectory(const QString& dirPath) {
    m_controller->loadDirectory(dirPath);
}

void ViewportPane::setupUi() {
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0,0,0,0);
    vbox->setSpacing(4);

    vbox->addWidget(m_viewport, 1);

    auto* bar = new QWidget(this);
    bar->setObjectName("transportBar");
    bar->setStyleSheet(kTransportQss);
    auto* hbox = new QHBoxLayout(bar);
    hbox->setContentsMargins(8, 6, 8, 6);
    hbox->setSpacing(6);

    const QColor kFg("#dfe1e5");   // standard glyph colour
    const QColor kOn("#ffffff");   // glyph colour on the accent button

    m_btnREW     = new QPushButton(bar);
    m_btnReverse = new QPushButton(bar);
    m_btnStop    = new QPushButton(bar);
    m_btnPlay    = new QPushButton(bar);
    m_btnFF      = new QPushButton(bar);
    m_btnLoad    = new QPushButton(bar);
    m_btnPlay->setObjectName("playBtn");

    styleButton(m_btnREW,     MediaIcons::rewind(kFg),      "Slower");
    styleButton(m_btnReverse, MediaIcons::playReverse(kFg), "Play reverse");
    styleButton(m_btnStop,    MediaIcons::stop(kFg),        "Stop");
    styleButton(m_btnPlay,    MediaIcons::play(kOn),        "Play");
    styleButton(m_btnFF,      MediaIcons::fastForward(kFg), "Faster");
    styleButton(m_btnLoad,    MediaIcons::folder(kFg),      "Open image folder");

    m_status = new QLabel("Idle", bar);
    m_status->setMinimumWidth(360);

    m_filterBox = new QComboBox(bar);
    m_filterBox->setCursor(Qt::PointingHandCursor);
    m_filterBox->setFocusPolicy(Qt::NoFocus);
    m_filterBox->setToolTip("Display filter");
    m_filterBox->addItems({
         "Normal", "Grayscale", "Invert", "Edge", "Blur", "Pixelate", "Textured", "Sobel", "GeomOutline"
    });

    hbox->addWidget(m_btnREW);
    hbox->addWidget(m_btnReverse);
    hbox->addWidget(m_btnStop);
    hbox->addWidget(m_btnPlay);
    hbox->addWidget(m_btnFF);
    hbox->addSpacing(10);
    hbox->addWidget(m_filterBox);
    hbox->addStretch(2);
    hbox->addWidget(m_btnLoad);
    hbox->addStretch(1);
    hbox->addWidget(m_status);

    vbox->addWidget(bar, 0);
}

void ViewportPane::connectSignals() {
    connect(m_btnLoad, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, "Select image sequence directory");
        if (!dir.isEmpty()) m_controller->loadDirectory(dir);
    });
    connect(m_btnPlay, &QPushButton::clicked, m_controller, &MediaController::playForward);
    connect(m_btnReverse, &QPushButton::clicked, m_controller, &MediaController::playReverse);
    connect(m_btnStop, &QPushButton::clicked, m_controller, &MediaController::stop);
    connect(m_btnFF, &QPushButton::clicked, m_controller, &MediaController::faster);
    connect(m_btnREW, &QPushButton::clicked, m_controller, &MediaController::slower);
    connect(m_filterBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        m_viewport, &GLViewport::setFilterMode);

}


