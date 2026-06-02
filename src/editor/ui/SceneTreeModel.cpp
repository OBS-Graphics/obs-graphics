#include "SceneTreeModel.h"

#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "engine/scene.h"
#include "engine/graphic.h"
#include "engine/element.h"

#include <QDataStream>
#include <QIODevice>

SceneTreeModel::SceneTreeModel(SceneDocument* doc, QObject* parent)
    : QAbstractItemModel(parent)
    , m_doc(doc)
{
    connect(m_doc, &SceneDocument::documentChanged, this, [this]() {
        beginResetModel();
        endResetModel();
    });
}

QModelIndex SceneTreeModel::index(int row, int column,
                                  const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    if (!parent.isValid()) {
        // Root level: only one child = the scene node (row 0)
        if (row == 0)
            return createIndex(row, column, makeId(LEVEL_SCENE, -1, -1));
        return {};
    }

    quintptr pid = parent.internalId();
    quintptr plevel = levelOf(pid);

    if (plevel == LEVEL_SCENE) {
        // Children of scene = graphics
        return createIndex(row, column, makeId(LEVEL_GRAPHIC, row, -1));
    }

    if (plevel == LEVEL_GRAPHIC) {
        int gi = giOf(pid);
        return createIndex(row, column, makeId(LEVEL_ELEMENT, gi, row));
    }

    return {};
}

QModelIndex SceneTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return {};

    quintptr id    = child.internalId();
    quintptr level = levelOf(id);

    if (level == LEVEL_SCENE)
        return {};  // scene is child of invisible root

    if (level == LEVEL_GRAPHIC)
        return createIndex(0, 0, makeId(LEVEL_SCENE, -1, -1));

    if (level == LEVEL_ELEMENT) {
        int gi = giOf(id);
        return createIndex(gi, 0, makeId(LEVEL_GRAPHIC, gi, -1));
    }

    return {};
}

int SceneTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return 1; // one scene node under invisible root

    quintptr id    = parent.internalId();
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

    return 0; // elements have no children
}

int SceneTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

QVariant SceneTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return {};

    quintptr id    = index.internalId();
    quintptr level = levelOf(id);

    if (level == LEVEL_SCENE)
        return m_doc->sceneName();

    if (level == LEVEL_GRAPHIC) {
        int gi = giOf(id);
        const Scene& s = m_doc->scene();
        if (gi < 0 || gi >= static_cast<int>(s.graphics.size()))
            return {};
        const Graphic& g = s.graphics[gi];
        return QString("[%1] %2")
            .arg(g.zOrder)
            .arg(QString::fromStdString(g.id));
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
        const Element& el = g.elements[ei];
        const char* typeName = (el.type == ElementType::Text) ? "text" : "rectangle";
        return QString("[%1] %2 (%3)")
            .arg(el.zOrder)
            .arg(QString::fromStdString(el.id))
            .arg(typeName);
    }

    return {};
}

Qt::ItemFlags SceneTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled; // allow drop on root to re-order

    quintptr id    = index.internalId();
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
    // Encode as 8-byte big-endian
    for (int i = 7; i >= 0; --i) {
        data[i] = static_cast<char>(id & 0xFF);
        id >>= 8;
    }

    mime->setData("application/x-obs-scene-node", data);
    return mime;
}

bool SceneTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                  int row, int /*column*/,
                                  const QModelIndex& parent)
{
    if (action != Qt::MoveAction)
        return false;
    if (!data->hasFormat("application/x-obs-scene-node"))
        return false;

    QByteArray encoded = data->data("application/x-obs-scene-node");
    if (encoded.size() < 8)
        return false;

    // Decode 8-byte big-endian
    quintptr srcId = 0;
    for (int i = 0; i < 8; ++i) {
        srcId = (srcId << 8) | static_cast<unsigned char>(encoded[i]);
    }

    quintptr srcLevel = levelOf(srcId);
    int srcGi = giOf(srcId);
    int srcEi = eiOf(srcId);

    const Scene& s = m_doc->scene();

    if (srcLevel == LEVEL_GRAPHIC) {
        // Dropping a graphic — determine destination row
        int destRow = row;
        if (!parent.isValid()) {
            // Dropped on root; treat as last graphic
            destRow = static_cast<int>(s.graphics.size()) - 1;
        } else {
            quintptr pid = parent.internalId();
            quintptr plevel = levelOf(pid);
            if (plevel == LEVEL_SCENE) {
                if (destRow < 0)
                    destRow = static_cast<int>(s.graphics.size()) - 1;
            } else if (plevel == LEVEL_GRAPHIC) {
                destRow = giOf(pid);
            } else {
                return false;
            }
        }

        if (destRow < 0) destRow = 0;
        if (destRow >= static_cast<int>(s.graphics.size()))
            destRow = static_cast<int>(s.graphics.size()) - 1;

        if (destRow == srcGi)
            return false;

        m_doc->undoStack()->push(new MoveGraphicCmd(m_doc, srcGi, destRow));
        return true;
    }

    if (srcLevel == LEVEL_ELEMENT) {
        // Dropping an element — must stay within same graphic
        if (!parent.isValid())
            return false;

        quintptr pid = parent.internalId();
        quintptr plevel = levelOf(pid);

        int destGi = -1;
        int destRow = row;

        if (plevel == LEVEL_GRAPHIC) {
            destGi = giOf(pid);
        } else if (plevel == LEVEL_ELEMENT) {
            // Dropped onto another element within same graphic
            destGi = giOf(pid);
            destRow = eiOf(pid);
        } else {
            return false;
        }

        if (destGi != srcGi)
            return false; // cross-graphic moves not supported

        if (destGi < 0 || destGi >= static_cast<int>(s.graphics.size()))
            return false;

        const Graphic& g = s.graphics[destGi];
        if (destRow < 0) destRow = 0;
        if (destRow >= static_cast<int>(g.elements.size()))
            destRow = static_cast<int>(g.elements.size()) - 1;

        if (destRow == srcEi)
            return false;

        m_doc->undoStack()->push(new MoveElementCmd(m_doc, srcGi, srcEi, destRow));
        return true;
    }

    return false;
}

SelectionId SceneTreeModel::selectionIdFor(const QModelIndex& index) const
{
    if (!index.isValid())
        return SelectionId{};

    quintptr id    = index.internalId();
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

    case SelectionId::Level::Graphic:
        return createIndex(sel.graphicIndex, 0,
                           makeId(LEVEL_GRAPHIC, sel.graphicIndex, -1));

    case SelectionId::Level::Element:
        return createIndex(sel.elementIndex, 0,
                           makeId(LEVEL_ELEMENT, sel.graphicIndex, sel.elementIndex));

    default:
        return {};
    }
}
