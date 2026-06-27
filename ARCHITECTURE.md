# Architecture

`QtOpenGLQuadPlayers` runs four independent image-sequence players in a 2×2 grid.
The design goal is simple: **the GUI thread never blocks on disk I/O or image
decoding**, no matter how large the sequences are. Each pane owns a worker thread
that decodes frames; the GUI thread only ever does the cheap, mandatory work of
uploading a texture and drawing.

![Concurrency architecture](docs/architecture.png)

> The diagram source is [`docs/architecture.svg`](docs/architecture.svg) (vector,
> editable in draw.io / Excalidraw / any browser); the PNG above is exported from it.

## Threading model

Each [`ViewportPane`](ViewportPane.cpp) wires together three objects:

| Object | Thread | Responsibility |
| --- | --- | --- |
| [`DecodeWorker`](DecodeWorker.cpp) | its own `QThread` | scan directory, pace playback, decode frames |
| [`FrameQueue`](FrameQueue.h) | shared (lock-guarded) | hand decoded frames across the thread boundary |
| [`GLViewport`](GLViewport.cpp) | GUI thread | pull freshest frame, upload texture, render |

[`MediaController`](MediaController.cpp) is the GUI-thread facade that owns the
worker's `QThread` and the queue. It does **not** call the worker directly —
every user action (`loadDirectory`, `playForward`, `faster`, …) is emitted as a
`request*` signal connected to the worker's slot. Because the worker lives in a
different thread, Qt delivers these as **queued** calls that execute *inside* the
worker thread. The worker's outputs (`frameAvailable`, `metricsUpdated`,
`statusText`) travel back the same way.

The data flow for one frame:

```
QTimer tick (worker thread)
  → DecodeWorker advances index, QImageReader decodes  ── off the GUI thread
  → FrameQueue.push(frame)                              ── drop-oldest if full
  → emit frameAvailable()                               ── queued signal → GUI thread
  → GLViewport::consumeFrames() (GUI thread)
  → FrameQueue.popLatest()                              ── take newest, discard stale
  → glTexImage2D / glTexSubImage2D                      ── GL context lives here
  → update() → paintGL()
```

The only state shared between threads is the `FrameQueue`, and it is fully
mutex-guarded. Decoded `QImage`s are the only thing that crosses the boundary;
**no OpenGL call ever leaves the GUI thread**, because a `QOpenGLWidget`'s context
has thread affinity to the GUI thread. Violating that is the classic source of
silent GL corruption, so the boundary is drawn deliberately at the texture upload.

## Why the queue is bounded + drop-oldest

A naïve hand-off — emitting a queued `frameReady(QImage)` signal per frame — works
until the consumer falls behind. Then the event queue grows without bound: every
decoded frame piles up as a pending event, memory balloons, and displayed frames
lag further and further behind real time. That is exactly the failure mode a media
player must avoid.

[`FrameQueue`](FrameQueue.h) fixes this with two rules:

- **Bounded (capacity 3).** `push()` never lets the buffer exceed capacity; if it's
  full, it discards the **oldest** frame to make room and reports the drop. Memory
  is therefore capped regardless of how slow the consumer is.
- **Consumer takes the freshest.** `popLatest()` returns the newest queued frame and
  discards any older ones still buffered. Under load the player sheds frames instead
  of accumulating latency — you always see *now*, not a growing backlog.

This is the right trade-off for playback: a dropped frame is invisible, but a
growing delay between the timeline and the picture is not. The dropped-frame count
is surfaced live in each pane's metrics, so backpressure is observable rather than
hidden.

## Shutdown ordering

Tearing down a thread that owns timers and touches a shared queue has to happen in
a specific order, or you get either a deadlock or a use-after-free. `~MediaController`
([MediaController.cpp](MediaController.cpp)) does:

1. **Stop the worker's timers, in the worker thread.**
   `QMetaObject::invokeMethod(worker, "shutdown", Qt::BlockingQueuedConnection)`
   runs `shutdown()` *inside* the worker thread and blocks until it returns. This
   guarantees no `QTimer` can fire — and therefore no new `push()` / `frameAvailable`
   — after this point. (Safe from deadlock because the caller is the GUI thread and
   the receiver is a different, still-running thread.)
2. **Quit and join.** `thread->quit()` asks the worker's event loop to exit;
   `thread->wait()` blocks until the thread has fully finished.
3. **Auto-delete the worker.** A `connect(thread, &QThread::finished, worker,
   &QObject::deleteLater)` made at construction destroys the worker as the thread
   winds down — on the worker's own thread, as Qt requires.
4. **Delete the queue.** Only after `wait()` returns — i.e. once the producer thread
   is provably gone — is the shared `FrameQueue` freed, so there is no window in
   which a live worker could touch a deleted queue.

Because each pane's `MediaController` is a child `QObject` of its `ViewportPane`,
closing the window destroys the panes, which runs this sequence for all four
workers. Verified: a graceful window close exits with code 0 and no deadlock.
