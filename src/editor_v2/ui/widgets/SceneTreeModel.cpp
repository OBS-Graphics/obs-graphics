#include "SceneTreeModel.h"

#include "engine/element.h"
#include "engine/graphic.h"
#include "engine/scene.h"
#include "icons.h"
#include "model/SceneDocument.h"
#include "model/UndoCommands.h"

#include <QDataStream>
#include <QIODevice>
#include <algorithm>
#include <numeric>

SceneTreeModel::SceneTreeModel(SceneDocument* doc, QObject* parent)
    : QAbstractItemModel(parent), m_doc(doc)
{
    connect(m_doc, &SceneDocument::documentChanged, this, [this]() {
        beginResetModel();
        endResetModel();
    });
}

// ── Z-Order sort helpers ──────────────────────────────────────────────────────

QVector<int> SceneTreeModel::sortedGraphicIndices() const
{
    int n = static_cast<int>(m_doc->scene().graphics.size());
    QVector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    // Descending: highest zOrder → row 0 (topmost in the layer list)
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        return m_doc->scene().graphics[a].zOrder > m_doc->scene().graphics[b].zOrder;
    });
    return idx;
}

QVector<int> SceneTreeModel::sortedElementIndices(int gi) const
{
    const Scene& s = m_doc->scene();
    if (gi < 0 || gi >= static_cast<int>(s.graphics.size()))
        return {};
    int n = static_cast<int>(s.graphics[gi].elements.size());
    QVector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        return s.graphics[gi].elements[a].zOrder > s.graphics[gi].elements[b].zOrder;
    });
    return idx;
}

int SceneTreeModel::sortedGraphicRow(int gi) const
{
    auto sorted = sortedGraphicIndices();
    for (int r = 0; r < sorted.size(); ++r)
        if (sorted[r] == gi)
            return r;
    return gi;
}

int SceneTreeModel::sortedElementRow(int gi, int ei) const
{
    auto sorted = sortedElementIndices(gi);
    for (int r = 0; r < sorted.size(); ++r)
        if (sorted[r] == ei)
            return r;
    return ei;
}

// ── QAbstractItemModel overrides ──────────────────────────────────────────────

QModelIndex SceneTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    if (!parent.isValid()) {
        if (row == 0)
            return createIndex(row, column, makeId(LEVEL_SCENE, -1, -1));
        return {};
    }

    quintptr pid = parent.internalId();
    quintptr plevel = levelOf(pid);

    if (plevel == LEVEL_SCENE) {
        auto sorted = sortedGraphicIndices();
        if (row >= sorted.size())
            return {};
        return createIndex(row, column, makeId(LEVEL_GRAPHIC, sorted[row], -1));
    }

    if (plevel == LEVEL_GRAPHIC) {
        int gi = giOf(pid);
        auto sorted = sortedElementIndices(gi);
        if (row >= sorted.size())
            return {};
        return createIndex(row, column, makeId(LEVEL_ELEMENT, gi, sorted[row]));
    }

    return {};
}

QModelIndex SceneTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return {};

    quintptr id = child.internalId();
    quintptr level = levelOf(id);

    if (level == LEVEL_SCENE)
        return {};

    if (level == LEVEL_GRAPHIC)
        return createIndex(0, 0, makeId(LEVEL_SCENE, -1, -1));

    if (level == LEVEL_ELEMENT) {
        int gi = giOf(id);
        int row = sortedGraphicRow(gi);
        return createIndex(row, 0, makeId(LEVEL_GRAPHIC, gi, -1));
    }

    return {};
}

int SceneTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return 1;

    quintptr id = parent.internalId();
    quintptr level = levelOf(id);

    if (level == LEVEL_SCENE)
        return static_cast<int>(m_doc->scene().graphics.size());

    if (level == LEVEL_GRAPHIC) {
        int gi = giOf(id);
        const Scene& s = m_doc->scene();
        if (gi < 0 || gi >= static_cast<int>(s.graphics.size()))
            return 0;
        return static_cast<int>(s.graphics[gi].elements.size());
    }

    return 0;
}

int SceneTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

QVariant SceneTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    quintptr id = index.internalId();
    quintptr level = levelOf(id);

    if (role == Qt::DisplayRole) {
        if (level == LEVEL_SCENE)
            return m_doc->sceneName();

        if (level == LEVEL_GRAPHIC) {
            int gi = giOf(id);
            const Scene& s = m_doc->scene();
            if (gi < 0 || gi >= static_cast<int>(s.graphics.size()))
                return {};
            return QString::fromStdString(s.graphics[gi].id);
        }

        if (level == LEVEL_ELEMENT) {
            int gi = giOf(id);
            int ei = eiOf(id);
            const Scene& s = m_doc->scene();
            if (gi < 0 || gi >= static_cast<int>(s.graphics.size()))
                return {};
            const Graphic& g = s.graphics[gi];
            if (ei < 0 || ei >= static_cast<int>(g.elements.size()))
                return {};
            return QString::fromStdString(g.elements[ei].id);
        }
    }

    if (role == Qt::DecorationRole) {
        if (level == LEVEL_SCENE)
            return qta()->icon(fa::fa_solid, fa::fa_film);

        if (level == LEVEL_GRAPHIC)
            return qta()->icon(fa::fa_solid, fa::fa_layer_group);

        if (level == LEVEL_ELEMENT) {
            int gi = giOf(id);
            int ei = eiOf(id);
            const Scene& s = m_doc->scene();
            if (gi < 0 || gi >= static_cast<int>(s.graphics.size()))
                return {};
            const Graphic& g = s.graphics[gi];
            if (ei < 0 || ei >= static_cast<int>(g.elements.size()))
                return {};
            if (g.elements[ei].type == ElementType::Text)
                return qta()->icon(fa::fa_solid, fa::fa_font);
            return qta()->icon(fa::fa_solid, fa::fa_square);
        }
    }

    return {};
}

Qt::ItemFlags SceneTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;

    quintptr id = index.internalId();
    quintptr level = levelOf(id);

    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (level == LEVEL_GRAPHIC || level == LEVEL_ELEMENT)
        f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    return f;
}

Qt::DropActions SceneTreeModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList SceneTreeModel::mimeTypes() const
{
    return {"application/x-obs-scene-node"};
}

QMimeData* SceneTreeModel::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.isEmpty())
        return nullptr;

    auto* mime = new QMimeData;
    QByteArray data;
    data.resize(8);

    quintptr id = indexes.first().internalId();
    for (int i = 7; i >= 0; --i) {
        data[i] = static_cast<char>(id & 0xFF);
        id >>= 8;
    }

    mime->setData("application/x-obs-scene-node", data);
    return mime;
}

bool SceneTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                  int /*column*/, const QModelIndex& parent)
{
    if (action != Qt::MoveAction)
        return false;
    if (!data->hasFormat("application/x-obs-scene-node"))
        return false;

    QByteArray encoded = data->data("application/x-obs-scene-node");
    if (encoded.size() < 8)
        return false;

    quintptr srcId = 0;
    for (int i = 0; i < 8; ++i)
        srcId = (srcId << 8) | static_cast<unsigned char>(encoded[i]);

    quintptr srcLevel = levelOf(srcId);
    int srcGi = giOf(srcId);
    int srcEi = eiOf(srcId);

    const Scene& s = m_doc->scene();

    if (srcLevel == LEVEL_GRAPHIC) {
        // Reorder by updating Z-Order values
        auto sorted = sortedGraphicIndices(); // current sorted order (desc by zOrder)
        int n = sorted.size();
        if (n < 2)
            return false;

        int srcRow = sortedGraphicRow(srcGi);

        // Determine destination row
        int destRow = row;
        if (destRow < 0 || (!parent.isValid() && row < 0))
            destRow = n - 1; // drop at end
        else if (parent.isValid()) {
            quintptr pid = parent.internalId();
            if (levelOf(pid) == LEVEL_GRAPHIC)
                destRow = sortedGraphicRow(giOf(pid));
        }
        destRow = std::clamp(destRow, 0, n - 1);

        if (destRow == srcRow)
            return false;

        // Build new order: remove src, insert at dest
        QVector<int> newOrder = sorted;
        newOrder.remove(srcRow);
        int insertPos = (destRow > srcRow) ? destRow - 1 : destRow;
        insertPos = std::clamp(insertPos, 0, n - 1);
        newOrder.insert(insertPos, srcGi);

        // Assign Z-Orders: position 0 (top) → zOrder N-1, position N-1 → zOrder 0
        auto* macro = new QUndoCommand("reorder graphic");
        for (int r = 0; r < n; ++r) {
            int gi = newOrder[r];
            int newZ = n - 1 - r;
            if (s.graphics[gi].zOrder != newZ) {
                new SetGraphicFieldCmd<int>(
                    m_doc, s.graphics[gi].id, newZ, [](Graphic& g) -> int& { return g.zOrder; },
                    "zOrder", macro);
            }
        }
        m_doc->undoStack()->push(macro);
        return true;
    }

    if (srcLevel == LEVEL_ELEMENT) {
        if (!parent.isValid())
            return false;

        quintptr pid = parent.internalId();
        quintptr plevel = levelOf(pid);

        int destGi = -1;
        int destRow = row;

        if (plevel == LEVEL_GRAPHIC) {
            destGi = giOf(pid);
        } else if (plevel == LEVEL_ELEMENT) {
            destGi = giOf(pid);
            destRow = sortedElementRow(destGi, eiOf(pid));
        } else {
            return false;
        }

        if (destGi != srcGi)
            return false;
        if (destGi < 0 || destGi >= static_cast<int>(s.graphics.size()))
            return false;

        const Graphic& g = s.graphics[destGi];
        int n = static_cast<int>(g.elements.size());
        if (n < 2)
            return false;

        auto sorted = sortedElementIndices(destGi);
        int srcRow = sortedElementRow(destGi, srcEi);

        if (destRow < 0)
            destRow = n - 1;
        destRow = std::clamp(destRow, 0, n - 1);
        if (destRow == srcRow)
            return false;

        QVector<int> newOrder = sorted;
        newOrder.remove(srcRow);
        int insertPos = (destRow > srcRow) ? destRow - 1 : destRow;
        insertPos = std::clamp(insertPos, 0, n - 1);
        newOrder.insert(insertPos, srcEi);

        const std::string gid = g.id;
        auto* macro = new QUndoCommand("reorder element");
        for (int r = 0; r < n; ++r) {
            int ei = newOrder[r];
            int newZ = n - 1 - r;
            if (g.elements[ei].zOrder != newZ) {
                new SetElementFieldCmd<int>(
                    m_doc, gid, g.elements[ei].id, newZ,
                    [](Element& e) -> int& { return e.zOrder; }, "zOrder", -1, macro);
            }
        }
        m_doc->undoStack()->push(macro);
        return true;
    }

    return false;
}

SelectionId SceneTreeModel::selectionIdFor(const QModelIndex& index) const
{
    if (!index.isValid())
        return SelectionId{};

    quintptr id = index.internalId();
    quintptr level = levelOf(id);

    if (level == LEVEL_SCENE)
        return SelectionId{SelectionId::Level::Scene, -1, -1};

    if (level == LEVEL_GRAPHIC)
        return SelectionId{SelectionId::Level::Graphic, giOf(id), -1};

    if (level == LEVEL_ELEMENT)
        return SelectionId{SelectionId::Level::Element, giOf(id), eiOf(id)};

    return SelectionId{};
}

QModelIndex SceneTreeModel::indexForSelection(const SelectionId& sel) const
{
    switch (sel.level) {
    case SelectionId::Level::Scene:
        return createIndex(0, 0, makeId(LEVEL_SCENE, -1, -1));

    case SelectionId::Level::Graphic: {
        int row = sortedGraphicRow(sel.graphicIndex);
        return createIndex(row, 0, makeId(LEVEL_GRAPHIC, sel.graphicIndex, -1));
    }

    case SelectionId::Level::Element: {
        int row = sortedElementRow(sel.graphicIndex, sel.elementIndex);
        return createIndex(row, 0, makeId(LEVEL_ELEMENT, sel.graphicIndex, sel.elementIndex));
    }

    default:
        return {};
    }
}
