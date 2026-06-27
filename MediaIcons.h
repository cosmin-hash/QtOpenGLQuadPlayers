#pragma once
#include <QIcon>
#include <QColor>

// Crisp, resolution-independent media-transport glyphs drawn with QPainter so
// the UI needs no external image assets and stays sharp at any DPI.
namespace MediaIcons {
    QIcon play(const QColor& c);
    QIcon playReverse(const QColor& c);
    QIcon pause(const QColor& c);
    QIcon stop(const QColor& c);
    QIcon rewind(const QColor& c);       // |◀◀  slower / seek back
    QIcon fastForward(const QColor& c);  // ▶▶|  faster / seek forward
    QIcon folder(const QColor& c);
}
