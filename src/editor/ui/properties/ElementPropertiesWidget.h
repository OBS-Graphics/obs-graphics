#pragma once

#include <QWidget>
#include "engine/animation.h"

class SceneDocument;
class AnimationWidget;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;

// Merge tag constants for SetElementFieldCmd to collapse rapid spinbox edits
namespace ElemMergeTag {
    enum : int {
        ZOrder    = 1000,
        X         = 1001,
        Y         = 1002,
        W         = 1003,
        H         = 1004,
        Rotation  = 1005,
        Opacity   = 1006,
        StrokeW   = 1007,
    };
}

class ElementPropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit ElementPropertiesWidget(SceneDocument* doc, QWidget* parent = nullptr);

    void setSelection(int gi, int ei);
    void refresh();   // called by subclasses after their own refresh

protected:
    SceneDocument* m_doc;
    int            m_gi{-1};
    int            m_ei{-1};
    bool           m_updating{false};

private slots:
    void onDocumentChanged();
    void onIdEditingFinished();
    void onZOrderChanged(int value);
    void onXChanged(double value);
    void onYChanged(double value);
    void onWChanged(double value);
    void onHChanged(double value);
    void onRotationChanged(double value);
    void onOpacityChanged(double value);
    void onStrokeWidthChanged(double value);
    void onMaskChanged(int index);
    void onParentChanged(int index);
    void onAnimInChanged(AnimationDef def);
    void onAnimOutChanged(AnimationDef def);

private:
    void populateRefCombos();

    QLineEdit*       m_idEdit;
    QSpinBox*        m_zOrderSpin;
    QDoubleSpinBox*  m_xSpin;
    QDoubleSpinBox*  m_ySpin;
    QDoubleSpinBox*  m_wSpin;
    QDoubleSpinBox*  m_hSpin;
    QDoubleSpinBox*  m_rotationSpin;
    QDoubleSpinBox*  m_opacitySpin;
    QDoubleSpinBox*  m_strokeWidthSpin;
    QComboBox*       m_maskCombo;
    QComboBox*       m_parentCombo;
    AnimationWidget* m_animIn;
    AnimationWidget* m_animOut;
};
