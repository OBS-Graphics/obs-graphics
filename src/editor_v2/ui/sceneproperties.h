#pragma once

#include <QWidget>

class SceneDocument;
class QComboBox;
class QLineEdit;
class QSpinBox;

class SceneProperties : public QWidget {
    Q_OBJECT
public:
    explicit SceneProperties(SceneDocument* doc, QWidget* parent = nullptr);

private slots:
    void onDocumentChanged();
    void onNameEditingFinished();
    void onPresetChanged(int index);
    void onWidthFinished();
    void onHeightFinished();

private:
    SceneDocument* m_doc;
    QLineEdit*     m_nameEdit;
    QComboBox*     m_presetCombo;
    QSpinBox*      m_widthSpin;
    QSpinBox*      m_heightSpin;
    bool           m_updating{false};

    struct Preset { const char* label; int w; int h; };
    static const Preset kPresets[];
    static constexpr int kCustomIndex = 5;

    int findPresetIndex(int w, int h) const;
};
