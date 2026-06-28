#pragma once
#include <QObject>
#include <QStringList>
#include <QElapsedTimer>
#include <QMetaType>

class QTimer;
class FrameQueue;

// Runs on its own QThread. Owns the playback timing, scans the image
// directory, decodes frames off the GUI thread, and pushes them into the
// shared FrameQueue. All public slots are meant to be invoked via queued
// connections from the GUI thread.
class DecodeWorker : public QObject {
    Q_OBJECT
public:
    struct Metrics {
        double  fps           = 0.0;   // frames decoded & pushed per second
        double  avgDecodeMs   = 0.0;   // mean decode time over the interval
        int     queueDepth    = 0;     // frames currently buffered
        int     queueCapacity = 0;
        qint64  dropped       = 0;     // cumulative frames dropped (backpressure)
        qint64  index         = 0;     // current frame index
        int     total         = 0;     // total frames in the sequence
        double  speed         = 1.0;
        int     direction     = 0;     // -1 reverse, 0 stopped, +1 forward
    };

    explicit DecodeWorker(FrameQueue* queue, QObject* parent = nullptr);
    ~DecodeWorker() override;

public slots:
    void initialize();                       // creates timers inside the worker thread
    void setDirectory(const QString& dirPath);
    void playForward();
    void playReverse();
    void stop();
    void faster();
    void slower();
    void shutdown();                         // stop timers before the thread quits

signals:
    void frameAvailable();                   // a new frame is in the queue
    void metricsUpdated(const DecodeWorker::Metrics& m);
    void statusText(const QString& s);

private slots:
    void onTick();
    void onMetricsTick();

private:
    void restartTimer();
    void decodeCurrent();
    static QStringList scanImages(const QString& dirPath);

    FrameQueue* m_queue = nullptr;           // shared, not owned
    QStringList m_files;
    qint64      m_index = 0;
    double      m_speed = 1.0;
    int         m_dir   = 0;

    QTimer* m_timer        = nullptr;        // playback pacing
    QTimer* m_metricsTimer = nullptr;        // 1 Hz metrics emit

    // Metrics accumulators for the current interval.
    double         m_decodeAccumMs = 0.0;
    int            m_decodeCount   = 0;
    int            m_framesPushed  = 0;
    qint64         m_dropped       = 0;
    QElapsedTimer  m_intervalClock;
};

Q_DECLARE_METATYPE(DecodeWorker::Metrics)
