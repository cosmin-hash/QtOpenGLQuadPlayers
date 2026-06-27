#pragma once
#include <QObject>
#include "DecodeWorker.h"   // for DecodeWorker::Metrics used in signals

class QThread;
class FrameQueue;

// GUI-thread facade that owns a decode worker running on its own thread plus
// the frame queue shared with the viewport. The public slots simply marshal
// user actions across to the worker; worker signals are relayed back out.
class MediaController : public QObject {
    Q_OBJECT
public:
    explicit MediaController(QObject* parent = nullptr);
    ~MediaController() override;

    FrameQueue* frameQueue() const { return m_queue; }

public slots:
    void loadDirectory(const QString& dirPath);
    void playForward();
    void playReverse();
    void stop();
    void faster();
    void slower();

signals:
    // Relayed from the worker thread to the GUI thread (queued).
    void frameAvailable();
    void metricsUpdated(const DecodeWorker::Metrics& m);
    void statusText(const QString& text);

    // Dispatched from the GUI thread into the worker thread (queued).
    void requestDirectory(const QString& dirPath);
    void requestPlayForward();
    void requestPlayReverse();
    void requestStop();
    void requestFaster();
    void requestSlower();

private:
    QThread*     m_thread = nullptr;
    DecodeWorker* m_worker = nullptr;
    FrameQueue*  m_queue  = nullptr;
};
