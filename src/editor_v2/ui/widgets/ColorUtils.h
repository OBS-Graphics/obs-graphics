#pragma once
#include <QColor>
#include <QGradient>

QColor interpolateColor(const QColor& a, const QColor& b, qreal t);
QColor sampleGradient(const QGradientStops& stops, qreal t);
