#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>

#include <QObject>
#include <QString>
#include <QUndoStack>

#include "engine/scene.h"
#include "engine/json.hpp"

struct ElementRef {
    QString graphicsId, elementId;
};

class SceneDocument : public QObject {
    Q_OBJECT
public:
    explicit SceneDocument(QObject* parent = nullptr);

    // File operations
    bool load(const QString& path);
    bool save();
    bool saveAs(const QString& path);

    // Accessors
    Scene& scene() { return m_scene; }
    Element& getElement(ElementRef ref);
    QString filePath() const { return m_filePath; }
    bool isModified() const { return m_modified; }
    QString sceneName() const;

    QUndoStack* undoStack() { return &m_undoStack; }

    // Reset to an empty scene, clearing file path and undo stack
    void reset();

    // Update the mask/parent ID refs for one element and re-resolve pointers.
    // Both IDs are element IDs within the same graphic; pass "" for no reference.
    void setElementRef(const std::string& graphicId,
                       const std::string& elementId,
                       const std::string& maskId,
                       const std::string& parentId);

    // Move all element refs from one graphic ID key to another (for rename).
    void renameGraphicRef(const std::string& oldId, const std::string& newId);

    // All mutations go through here; fn may freely modify m_scene
    void applyMutation(std::function<void(Scene&)> fn);

    // Expose scene name mutation (used by SetSceneNameCmd)
    void setSceneName(const QString& name);

    // Serialization
    static nlohmann::json sceneToJson(const Scene& scene,
                                      const std::string& name = "");

signals:
    void documentChanged();
    void filePathChanged(const QString& path);
    void modifiedChanged(bool modified);

private:
    Scene      m_scene;
    QUndoStack m_undoStack;
    QString    m_filePath;
    bool       m_modified{false};
    std::string m_sceneName;

    // Rebuild Element::mask / Element::parent raw pointers after any
    // structural change.  We store the logical ids in parallel maps that
    // are populated from the JSON on load and kept in sync by undo commands.
    //
    // Key:   element id string
    // Value: (maskId, parentId) strings – empty = no reference
    using RefMap = std::map<std::string /*elementId*/,
                            std::pair<std::string /*maskId*/,
                                      std::string /*parentId*/>>;
    std::map<std::string /*graphicId*/, RefMap> m_elementRefs;

    void resolveElementPointers();
    void setModified(bool modified);

    // Helper: find element by id inside a Graphic (nullptr if not found)
    static Element* findElementById(Graphic& g, const std::string& id);
};
