#pragma once

#include <QAbstractItemModel>
#include <QMimeData>
#include <QVector>

#include "model/EditorScene.h"

class SceneDocument;

class SceneTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit SceneTreeModel(SceneDocument* doc, QObject* parent = nullptr);

    // QAbstractItemModel overrides
    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Drag-and-drop
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

    // Selection helpers
    SelectionId selectionIdFor(const QModelIndex& index) const;
    QModelIndex indexForSelection(const SelectionId& sel) const;

private:
    SceneDocument* m_doc;

    // Z-Order sorting helpers (items displayed descending by zOrder — highest first)
    QVector<int> sortedGraphicIndices() const;
    QVector<int> sortedElementIndices(int gi) const;
    int sortedGraphicRow(int gi) const;
    int sortedElementRow(int gi, int ei) const;

    static constexpr quintptr LEVEL_SCENE = 0;
    static constexpr quintptr LEVEL_GRAPHIC = 1;
    static constexpr quintptr LEVEL_ELEMENT = 2;

    static quintptr makeId(quintptr level, int gi, int ei)
    {
        return (level << 60) | (quintptr(gi & 0x0FFFFFFF) << 32) | quintptr(quint32(ei));
    }

    static quintptr levelOf(quintptr id)
    {
        return (id >> 60) & 0x3;
    }
    static int giOf(quintptr id)
    {
        return int((id >> 32) & 0x0FFFFFFF);
    }
    static int eiOf(quintptr id)
    {
        return int(qint32(id & 0xFFFFFFFF));
    }
};
