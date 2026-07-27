/*
StreamCanvas — Animated broadcast graphics source for OBS Studio
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

#include "ui-util.h"

#include <QSize>
#include <filesystem>

QToolButton* makeIconButton(Icons16 id, const QString& tooltip)
{
    auto* btn = new QToolButton();
    btn->setIcon(themedIcon(id));
    btn->setIconSize(QSize(16, 16));
    btn->setAutoRaise(true);
    btn->setFixedWidth(28);
    btn->setToolTip(tooltip);
    return btn;
}

QString displayNameForPath(const QString& path, const QString& fallback)
{
    QString name = QString::fromStdString(
        std::filesystem::path(path.toStdString()).stem().string());
    return name.isEmpty() ? fallback : name;
}
