/*
obs-graphics — Animated broadcast graphics source for OBS Studio
Copyright (C) 2026 Diego Lopes <diego95lopes@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include "icons.h"

#include <QApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>
#include <oclero/qlementine/icons/QlementineIcons.hpp>

namespace {

class PaletteIconEngine : public QIconEngine {
public:
    explicit PaletteIconEngine(const char* path) : m_path(path) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode, QIcon::State) override
    {
        painter->drawPixmap(rect, tinted(rect.size()));
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode, QIcon::State) override
    {
        return tinted(size);
    }

    QIconEngine* clone() const override { return new PaletteIconEngine(m_path); }

private:
    QPixmap tinted(const QSize& size) const
    {
        QSvgRenderer renderer{QString(m_path)};
        QSize target = size.isEmpty() ? renderer.defaultSize() : size;
        QPixmap px(target);
        px.fill(Qt::transparent);
        QPainter p(&px);
        renderer.render(&p);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(px.rect(), qApp->palette().windowText().color());
        return px;
    }

    const char* m_path;
};

} // namespace

void initIcons()
{
    initializeIconTheme();
}

QIcon themedIcon(Icons16 id)
{
    return QIcon(new PaletteIconEngine(iconPath(id)));
}
