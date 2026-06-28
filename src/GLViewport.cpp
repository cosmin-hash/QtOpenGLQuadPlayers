#include "GLViewport.h"
#include "FrameQueue.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QTimer>

static const char* VS_QUAD = R"(#version 410 core
layout(location=0) in vec2 inPos;
layout(location=1) in vec2 inUV;
out vec2 vUV;
uniform mat4 uModel;
void main(){
    vUV = inUV;
    gl_Position = uModel * vec4(inPos, 0.0, 1.0);
}
)";

static const char* FS_TEX = R"(#version 410 core
in vec2 vUV;
out vec4 outColor;
uniform sampler2D uTex;
uniform sampler2D uOverlay;   // for Textured mode
uniform int uFilterMode;      // 0..8 corresponds to UI
uniform vec2 uTexSize;        // texture resolution (w, h)
uniform float uOverlayMix;    // 0.0 unless Textured mode

// Helpers
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
vec3 sampleTex(vec2 uv) { return texture(uTex, uv).rgb; }

vec3 fNormal(vec2 uv) {
    return sampleTex(uv);
}

vec3 fGrayscale(vec2 uv) {
    float g = luma(sampleTex(uv));
    return vec3(g);
}

vec3 fInvert(vec2 uv) {
    return vec3(1.0) - sampleTex(uv);
}

// Simple edge: gradient magnitude from screen-space derivatives
vec3 fEdge(vec2 uv) {
    float y = luma(sampleTex(uv));
    float ex = dFdx(y);
    float ey = dFdy(y);
    float e = clamp(sqrt(ex*ex + ey*ey) * 2.0, 0.0, 1.0);
    return vec3(e);
}

// 3x3 box blur
vec3 fBlur(vec2 uv) {
    vec2 px = 1.0 / uTexSize;
    vec3 sum = vec3(0.0);
    for (int j = -1; j <= 1; ++j)
    for (int i = -1; i <= 1; ++i) {
        sum += sampleTex(uv + vec2(i, j) * px);
    }
    return sum / 9.0;
}

// Pixelate with fixed block size (adjustable)
vec3 fPixelate(vec2 uv) {
    // Block size in pixels (tweakable)
    const float block = 8.0;
    vec2 blockUV = floor(uv * uTexSize / block) * block / uTexSize;
    return sampleTex(blockUV + vec2(0.5) * block / uTexSize);
}

vec3 fTextured(vec2 uv) {
    vec3 base = sampleTex(uv);
    vec3 ov   = texture(uOverlay, uv).rgb;
    return mix(base, ov, uOverlayMix);
}

// Sobel edge detection on luma
vec3 fSobel(vec2 uv) {
    vec2 px = 1.0 / uTexSize;

    float tl = luma(sampleTex(uv + px * vec2(-1, -1)));
    float tc = luma(sampleTex(uv + px * vec2( 0, -1)));
    float tr = luma(sampleTex(uv + px * vec2( 1, -1)));
    float ml = luma(sampleTex(uv + px * vec2(-1,  0)));
    float mc = luma(sampleTex(uv));
    float mr = luma(sampleTex(uv + px * vec2( 1,  0)));
    float bl = luma(sampleTex(uv + px * vec2(-1,  1)));
    float bc = luma(sampleTex(uv + px * vec2( 0,  1)));
    float br = luma(sampleTex(uv + px * vec2( 1,  1)));

    float gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
    float gy = -tl - 2.0 * tc - tr + bl + 2.0 * bc + br;

    float edge = clamp(length(vec2(gx, gy)), 0.0, 1.0);
    return vec3(edge);
}

// Geometry outline approximation from local contrast
vec3 fGeomOutline(vec2 uv) {
    vec2 px = 1.0 / uTexSize;
    vec3 c  = sampleTex(uv);
    float diff = 0.0;
    diff += length(c - sampleTex(uv + px * vec2(-1, -1)));
    diff += length(c - sampleTex(uv + px * vec2( 0, -1)));
    diff += length(c - sampleTex(uv + px * vec2( 1, -1)));
    diff += length(c - sampleTex(uv + px * vec2(-1,  0)));
    diff += length(c - sampleTex(uv + px * vec2( 1,  0)));
    diff += length(c - sampleTex(uv + px * vec2(-1,  1)));
    diff += length(c - sampleTex(uv + px * vec2( 0,  1)));
    diff += length(c - sampleTex(uv + px * vec2( 1,  1)));
    float edge = clamp(diff / 8.0 * 1.5, 0.0, 1.0);
    return vec3(edge);
}

void main() {
    vec3 color;
    switch (uFilterMode) {
        case 0: color = fNormal(vUV);     break; // Normal
        case 1: color = fGrayscale(vUV);  break; // Grayscale
        case 2: color = fInvert(vUV);     break; // Invert
        case 3: color = fEdge(vUV);       break; // Edge
        case 4: color = fBlur(vUV);       break; // Blur
        case 5: color = fPixelate(vUV);   break; // Pixelate
        case 6: color = fTextured(vUV);   break; // Textured
        case 7: color = fSobel(vUV);      break; // Sobel
        case 8: color = fGeomOutline(vUV);break; // GeomOutline
        default: color = fNormal(vUV);    break;
    }
    outColor = vec4(color, 1.0);
}

)";

GLViewport::GLViewport(QWidget* parent)
    : QOpenGLWidget(parent)
{}

GLViewport::~GLViewport() {
    makeCurrent();
    if (m_tex) glDeleteTextures(1, &m_tex);
    if (m_overlayTex) glDeleteTextures(1, &m_overlayTex);
    delete m_prog;
    delete m_vbo;
    delete m_vao;
    doneCurrent();
}

void GLViewport::initializeGL() {
    initializeOpenGLFunctions();

    // Simple textured quad
    m_prog = new QOpenGLShaderProgram(this);
    m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex, VS_QUAD);
    m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment, FS_TEX);
    m_prog->link();

    // Fullscreen quad (two triangles)
    float verts[] = {
        // x, y,   u, v
        -1.f, -1.f, 0.f, 1.f,
         1.f, -1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 0.f,
         1.f,  1.f, 1.f, 0.f,
    };

    m_vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_vbo->create();
    m_vbo->bind();
    m_vbo->allocate(verts, sizeof(verts));

    m_vao = new QOpenGLVertexArrayObject(this);
    m_vao->create();
    m_vao->bind();
    m_vbo->bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    m_vao->release();
    m_vbo->release();

    glDisable(GL_DEPTH_TEST);
}

void GLViewport::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    // Aspect-fit is handled by scaling the quad via m_model in paintGL.
    updateAspect();
}

// Recompute the aspect-fit model matrix from the current texture and widget
// size. Letterboxes the image inside the quad by scaling down the longer axis.
void GLViewport::updateAspect() {
    if (m_texW == 0 || m_texH == 0 || height() == 0) {
        m_model.setToIdentity();
        return;
    }

    float imgAspect = float(m_texW) / float(m_texH);
    float winAspect = float(width()) / float(height());

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    if (winAspect > imgAspect) {
        scaleX = imgAspect / winAspect; // window wider than image
    } else {
        scaleY = winAspect / imgAspect; // window taller than image
    }

    m_model.setToIdentity();
    m_model.scale(scaleX, scaleY);
}

void GLViewport::paintGL() {
    glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_tex) return;

    m_prog->bind();

    // Texture 0: main frame
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    m_prog->setUniformValue("uTex", 0);

    // Texture 1: overlay (optional)
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_overlayTex ? m_overlayTex : 0);
    m_prog->setUniformValue("uOverlay", 1);

    // Filter uniforms
    m_prog->setUniformValue("uFilterMode", m_filterMode);
    m_prog->setUniformValue("uTexSize", QVector2D(float(m_texW), float(m_texH)));
    m_prog->setUniformValue("uOverlayMix", m_overlayMix);

    // Aspect-fit transform computed in updateAspect()
    m_prog->setUniformValue("uModel", m_model);

    m_vao->bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao->release();

    // Unbind
    glBindTexture(GL_TEXTURE_2D, 0);
    m_prog->release();
}


void GLViewport::consumeFrames() {
    if (!m_queue) return;
    DecodedFrame frame;
    int skipped = 0;
    // Take the freshest decoded frame; older queued ones are intentionally
    // dropped to minimise display latency.
    if (m_queue->popLatest(frame, skipped)) {
        setFrame(frame.image);
    }
}

void GLViewport::setFilterMode(int mode) {
    m_filterMode = mode;
    // Enable overlay mix only for Textured mode
    m_overlayMix = (m_filterMode == 6) ? 0.3f : 0.0f;
    update();
}

void GLViewport::setFrame(const QImage& img) {
    if (img.isNull()) return;

    makeCurrent();
    ensureTexture(img);
    uploadToTexture(img);
    doneCurrent();

    // Texture size may have changed; refresh the aspect-fit transform.
    updateAspect();
    update();
}

void GLViewport::ensureTexture(const QImage& img) {
    if (m_tex == 0) {
        glGenTextures(1, &m_tex);
        glBindTexture(GL_TEXTURE_2D, m_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_texW = m_texH = 0;
    }
}

void GLViewport::uploadToTexture(const QImage& img) {
    QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
    glBindTexture(GL_TEXTURE_2D, m_tex);

    if (m_texW != rgba.width() || m_texH != rgba.height()) {
        m_texW = rgba.width();
        m_texH = rgba.height();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_texW, m_texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_texW, m_texH, GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLViewport::setOverlayImage(const QImage& img) {
    if (img.isNull()) return;
    makeCurrent();
    ensureOverlay();
    QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
    glBindTexture(GL_TEXTURE_2D, m_overlayTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgba.width(), rgba.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
    glBindTexture(GL_TEXTURE_2D, 0);
    doneCurrent();
    update();
}

void GLViewport::ensureOverlay() {
    if (m_overlayTex == 0) {
        glGenTextures(1, &m_overlayTex);
        glBindTexture(GL_TEXTURE_2D, m_overlayTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

