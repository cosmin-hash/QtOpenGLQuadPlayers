#include "DecodeWorker.h"
#include "FrameQueue.h"
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <utility>

DecodeWorker::DecodeWorker(FrameQueue* queue, QObject* parent)
    : QObject(parent), m_queue(queue) {}

DecodeWorker::~DecodeWorker() = default;

void DecodeWorker::initialize() {
    // Created here (not in the constructor) so the timers' thread affinity is
    // the worker thread — this slot runs via QThread::started.
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &DecodeWorker::onTick);

    m_metricsTimer = new QTimer(this);
    m_metricsTimer->setInterval(1000);
    connect(m_metricsTimer, &QTimer::timeout, this, &DecodeWorker::onMetricsTick);
    m_metricsTimer->start();

    m_intervalClock.start();
}

void DecodeWorker::setDirectory(const QString& dirPath) {
    if (m_timer) m_timer->stop();
    m_dir   = 0;
    m_files = scanImages(dirPath);
    m_index = 0;
    m_dropped = 0;
    if (m_queue) m_queue->clear();

    if (m_files.isEmpty()) {
        emit statusText(QStringLiteral("No images found"));
        return;
    }
    emit statusText(QStringLiteral("Loaded %1 frames").arg(m_files.size()));
    decodeCurrent();   // show the first frame immediately
}

void DecodeWorker::playForward() {
    if (m_files.isEmpty()) return;
    m_dir = 1;
    restartTimer();
}

void DecodeWorker::playReverse() {
    if (m_files.isEmpty()) return;
    m_dir = -1;
    restartTimer();
}

void DecodeWorker::stop() {
    m_dir = 0;
    if (m_timer) m_timer->stop();
}

void DecodeWorker::faster() {
    const double speeds[] = {0.25, 0.5, 1.0, 2.0, 4.0};
    double next = speeds[4];
    for (double s : speeds) { if (s > m_speed) { next = s; break; } }
    m_speed = next;
    if (m_timer && m_timer->isActive()) restartTimer();
}

void DecodeWorker::slower() {
    const double speeds[] = {0.25, 0.5, 1.0, 2.0, 4.0};
    double prev = speeds[0];
    for (int i = 0; i < 5; ++i) {
        if (speeds[i] >= m_speed) { prev = (i > 0 ? speeds[i - 1] : speeds[0]); break; }
    }
    m_speed = prev;
    if (m_timer && m_timer->isActive()) restartTimer();
}

void DecodeWorker::shutdown() {
    if (m_timer)        m_timer->stop();
    if (m_metricsTimer) m_metricsTimer->stop();
}

void DecodeWorker::restartTimer() {
    if (!m_timer) return;
    const int interval = qMax(1, int(1000.0 / (24.0 * m_speed))); // base 24 fps
    m_timer->start(interval);
}

void DecodeWorker::onTick() {
    if (m_files.isEmpty()) return;
    m_index += m_dir;
    if (m_index >= m_files.size()) m_index = 0;
    if (m_index < 0)               m_index = m_files.size() - 1;
    decodeCurrent();
}

void DecodeWorker::decodeCurrent() {
    if (m_files.isEmpty()) return;

    QElapsedTimer clock;
    clock.start();
    QImageReader reader(m_files[m_index]);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    const double ms = clock.nsecsElapsed() / 1.0e6;
    if (img.isNull()) return;

    DecodedFrame frame;
    frame.image    = std::move(img);
    frame.index    = m_index;
    frame.decodeMs = ms;

    const int dropped = m_queue ? m_queue->push(std::move(frame)) : 0;

    m_dropped       += dropped;
    m_decodeAccumMs += ms;
    ++m_decodeCount;
    ++m_framesPushed;

    emit frameAvailable();
}

void DecodeWorker::onMetricsTick() {
    const double secs = m_intervalClock.isValid()
        ? m_intervalClock.nsecsElapsed() / 1.0e9
        : 1.0;

    Metrics m;
    m.fps           = secs > 0.0 ? m_framesPushed / secs : 0.0;
    m.avgDecodeMs   = m_decodeCount > 0 ? m_decodeAccumMs / m_decodeCount : 0.0;
    m.queueDepth    = m_queue ? m_queue->size() : 0;
    m.queueCapacity = m_queue ? m_queue->capacity() : 0;
    m.dropped       = m_dropped;
    m.index         = m_index;
    m.total         = m_files.size();
    m.speed         = m_speed;
    m.direction     = m_dir;
    emit metricsUpdated(m);

    m_framesPushed  = 0;
    m_decodeAccumMs = 0.0;
    m_decodeCount   = 0;
    m_intervalClock.restart();
}

QStringList DecodeWorker::scanImages(const QString& dirPath) {
    QDir dir(dirPath);
    const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp",
                                 "*.tif", "*.tiff", "*.webp"};
    QStringList files;
    const QFileInfoList list =
        dir.entryInfoList(filters, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const auto& fi : list) files << fi.absoluteFilePath();
    return files;
}
