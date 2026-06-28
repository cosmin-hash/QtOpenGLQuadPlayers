#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_1_Core>
#include <QMatrix4x4>
#include <QImage>

class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;
class FrameQueue;

class GLViewport : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core {
    Q_OBJECT
public:
    explicit GLViewport(QWidget* parent = nullptr);
    ~GLViewport() override;

    // Source of decoded frames produced by the worker thread.
    void setFrameQueue(FrameQueue* queue) { m_queue = queue; }

public slots:
    void consumeFrames();                     // pull freshest frame and upload (GUI thread)
    void setFrame(const QImage& img);
    void setFilterMode(int mode);
    void setOverlayImage(const QImage& img); // optional for Textured mode

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QOpenGLShaderProgram* m_prog = nullptr;
    QOpenGLBuffer* m_vbo = nullptr;
    QOpenGLVertexArrayObject* m_vao = nullptr;
    GLuint m_tex = 0;
    GLuint m_overlayTex = 0;  // optional overlay
    int m_texW = 0, m_texH = 0;
    float m_overlayMix = 0.0f; // 0 unless Textured mode
    int m_filterMode = 0;
    QMatrix4x4 m_model;        // aspect-fit transform applied in paintGL
    FrameQueue* m_queue = nullptr; // decoded-frame source (not owned)

    void ensureTexture(const QImage& img);
    void uploadToTexture(const QImage& img);
    void ensureOverlay();
    void uploadOverlay(const QImage& img);
    void updateAspect();       // recompute m_model from texture/widget size
};
