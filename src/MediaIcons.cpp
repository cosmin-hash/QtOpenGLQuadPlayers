#include "MediaIcons.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>

namespace {

constexpr int kCanvas = 64;   // draw large, let QIcon scale down crisply

// Render a filled path to a transparent icon.
QIcon render(const QPainterPath& path, const QColor& color) {
    QPixmap pm(kCanvas, kCanvas);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPath(path);
    p.end();
    return QIcon(pm);
}

// Triangle pointing right, filling the given bounding box.
QPainterPath triRight(qreal l, qreal t, qreal r, qreal b) {
    QPainterPath p;
    p.moveTo(l, t);
    p.lineTo(r, (t + b) / 2.0);
    p.lineTo(l, b);
    p.closeSubpath();
    return p;
}

// Triangle pointing left, filling the given bounding box.
QPainterPath triLeft(qreal l, qreal t, qreal r, qreal b) {
    QPainterPath p;
    p.moveTo(r, t);
    p.lineTo(l, (t + b) / 2.0);
    p.lineTo(r, b);
    p.closeSubpath();
    return p;
}

} // namespace

namespace MediaIcons {

QIcon play(const QColor& c) {
    return render(triRight(24, 16, 46, 48), c);
}

QIcon playReverse(const QColor& c) {
    return render(triLeft(18, 16, 40, 48), c);
}

QIcon pause(const QColor& c) {
    QPainterPath p;
    p.addRoundedRect(QRectF(22, 17, 8, 30), 2.5, 2.5);
    p.addRoundedRect(QRectF(34, 17, 8, 30), 2.5, 2.5);
    return render(p, c);
}

QIcon stop(const QColor& c) {
    QPainterPath p;
    p.addRoundedRect(QRectF(19, 19, 26, 26), 4, 4);
    return render(p, c);
}

QIcon rewind(const QColor& c) {
    QPainterPath p;
    p.addPath(triLeft(14, 19, 31, 45));
    p.addPath(triLeft(31, 19, 48, 45));
    return render(p, c);
}

QIcon fastForward(const QColor& c) {
    QPainterPath p;
    p.addPath(triRight(16, 19, 33, 45));
    p.addPath(triRight(33, 19, 50, 45));
    return render(p, c);
}

QIcon folder(const QColor& c) {
    QPainterPath body;
    body.addRoundedRect(QRectF(11, 24, 42, 26), 4, 4);
    QPainterPath tab;
    tab.addRoundedRect(QRectF(13, 18, 19, 11), 3, 3);
    return render(body.united(tab), c);
}

} // namespace MediaIcons
