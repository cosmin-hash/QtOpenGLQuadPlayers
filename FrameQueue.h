#pragma once
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <deque>
#include <utility>

// One decoded frame handed from a worker thread to the GUI thread.
struct DecodedFrame {
    QImage  image;
    qint64  index    = -1;
    double  decodeMs = 0.0;
};

// Bounded, thread-safe single-producer/single-consumer frame buffer.
// The producer (decode worker) drops the oldest frame when full so a slow
// consumer can never make memory grow without bound. The consumer takes the
// freshest frame to keep display latency low.
class FrameQueue {
public:
    explicit FrameQueue(int capacity = 3) : m_capacity(capacity) {}

    // Producer side. Returns the number of frames dropped to make room (0 or 1).
    int push(DecodedFrame frame) {
        QMutexLocker lock(&m_mutex);
        int dropped = 0;
        while (static_cast<int>(m_queue.size()) >= m_capacity) {
            m_queue.pop_front();
            ++dropped;
        }
        m_queue.push_back(std::move(frame));
        return dropped;
    }

    // Consumer side. Returns the most recent frame and discards any older ones
    // still queued; `skipped` reports how many were bypassed.
    bool popLatest(DecodedFrame& out, int& skipped) {
        QMutexLocker lock(&m_mutex);
        if (m_queue.empty()) return false;
        skipped = static_cast<int>(m_queue.size()) - 1;
        out = std::move(m_queue.back());
        m_queue.clear();
        return true;
    }

    int size() const {
        QMutexLocker lock(&m_mutex);
        return static_cast<int>(m_queue.size());
    }

    int capacity() const { return m_capacity; }

    void clear() {
        QMutexLocker lock(&m_mutex);
        m_queue.clear();
    }

private:
    mutable QMutex          m_mutex;
    std::deque<DecodedFrame> m_queue;
    int                     m_capacity;
};
