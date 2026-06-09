#include "ColorUtils.h"
#include <cmath>

QColor interpolateColor(const QColor& a, const QColor& b, qreal t)
{
    qreal a1 = a.alphaF(), a2 = b.alphaF();
    qreal r1 = a.redF() * a1, r2 = b.redF() * a2;
    qreal g1 = a.greenF() * a1, g2 = b.greenF() * a2;
    qreal b1 = a.blueF() * a1, b2 = b.blueF() * a2;

    qreal alpha = std::lerp(a1, a2, t);
    if (qFuzzyIsNull(alpha))
        return Qt::transparent;
    return QColor::fromRgbF(std::lerp(r1, r2, t) / alpha, std::lerp(g1, g2, t) / alpha,
                            std::lerp(b1, b2, t) / alpha, std::clamp(alpha, 0.0, 1.0));
}

QColor sampleGradient(const QGradientStops& stops, qreal t)
{
    if (stops.isEmpty())
        return Qt::transparent;
    if (t <= stops.first().first)
        return stops.first().second;
    if (t >= stops.last().first)
        return stops.last().second;

    for (int i = 1; i < stops.size(); i++) {
        if (t <= stops[i].first) {
            qreal span = stops[i].first - stops[i - 1].first;
            qreal local = (t - stops[i - 1].first) / span;
            return interpolateColor(stops[i - 1].second, stops[i].second, local);
        }
    }

    return Qt::transparent;
}
