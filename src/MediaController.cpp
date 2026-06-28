#include "MediaController.h"
#include "DecodeWorker.h"
#include "FrameQueue.h"
#include <QThread>

MediaController::MediaController(QObject* parent)
    : QObject(parent)
{
    // Required so Metrics can travel across the thread boundary in a queued
    // signal. Idempotent, so multiple panes calling it is fine.
    qRegisterMetaType<DecodeWorker::Metrics>("DecodeWorker::Metrics");

    m_queue  = new FrameQueue(3);
    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("DecodeWorker"));

    m_worker = new DecodeWorker(m_queue);   // no parent: owned via the thread
    m_worker->moveToThread(m_thread);

    // Build the worker's timers once its thread is running, and destroy the
    // worker when the thread has fully finished.
    connect(m_thread, &QThread::started,  m_worker, &DecodeWorker::initialize);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Worker -> GUI (auto-queued: affinities differ).
    connect(m_worker, &DecodeWorker::frameAvailable, this, &MediaController::frameAvailable);
    connect(m_worker, &DecodeWorker::metricsUpdated, this, &MediaController::metricsUpdated);
    connect(m_worker, &DecodeWorker::statusText,     this, &MediaController::statusText);

    // GUI -> worker (auto-queued: affinities differ).
    connect(this, &MediaController::requestDirectory,   m_worker, &DecodeWorker::setDirectory);
    connect(this, &MediaController::requestPlayForward, m_worker, &DecodeWorker::playForward);
    connect(this, &MediaController::requestPlayReverse, m_worker, &DecodeWorker::playReverse);
    connect(this, &MediaController::requestStop,        m_worker, &DecodeWorker::stop);
    connect(this, &MediaController::requestFaster,      m_worker, &DecodeWorker::faster);
    connect(this, &MediaController::requestSlower,      m_worker, &DecodeWorker::slower);

    m_thread->start();
}

MediaController::~MediaController() {
    if (m_thread) {
        // Stop the worker's timers deterministically *in its own thread* before
        // tearing the thread down, then join. The worker is deleted via the
        // finished()->deleteLater connection during thread shutdown.
        if (m_worker) {
            QMetaObject::invokeMethod(m_worker, "shutdown", Qt::BlockingQueuedConnection);
        }
        m_thread->quit();
        m_thread->wait();
    }
    delete m_queue;   // safe: worker thread has joined, no more producers
}

void MediaController::loadDirectory(const QString& dirPath) { emit requestDirectory(dirPath); }
void MediaController::playForward() { emit requestPlayForward(); }
void MediaController::playReverse() { emit requestPlayReverse(); }
void MediaController::stop()        { emit requestStop(); }
void MediaController::faster()      { emit requestFaster(); }
void MediaController::slower()      { emit requestSlower(); }
