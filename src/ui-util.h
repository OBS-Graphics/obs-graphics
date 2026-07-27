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

#pragma once

#include <QString>
#include <QToolButton>

#include "icons.h"

// Small Qt helpers shared by the dock and both settings dialogs. Everything
// here is presentation-only — no pool access, no I/O.

// The icon-only action button used in every table's actions cell (dock title
// rows, Data Sources rows). Centralized so the sizing/auto-raise styling is
// defined once rather than re-typed at each call site.
QToolButton* makeIconButton(Icons16 id, const QString& tooltip);

// File stem of `path` for display, or `fallback` when the stem is empty.
// Used for title names, data-source names and combo-box entries.
QString displayNameForPath(const QString& path, const QString& fallback);
