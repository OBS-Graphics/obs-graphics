#pragma once

#include <QObject>

class SceneDocument;

struct SelectionId {
    enum class Level { None, Scene, Graphic, Element };

    Level level{Level::None};
    int graphicIndex{-1};
    int elementIndex{-1};

    bool operator==(const SelectionId&) const = default;
};

class EditorScene : public QObject {
    Q_OBJECT
public:
    explicit EditorScene(SceneDocument* doc, QObject* parent = nullptr);

    SelectionId selection() const
    {
        return m_selection;
    }
    void setSelection(const SelectionId& id);

signals:
    void selectionChanged(SelectionId id);

private slots:
    void validateSelection();

private:
    SceneDocument* m_doc;
    SelectionId m_selection;
};
