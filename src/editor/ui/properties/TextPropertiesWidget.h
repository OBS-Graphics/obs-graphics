#pragma once

#include <QWidget>
#include "engine/types.hpp"

class SceneDocument;
class ElementPropertiesWidget;
class ColorPickerButton;
class QScrollArea;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QFontComboBox;
class QDoubleSpinBox;

class TextPropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit TextPropertiesWidget(SceneDocument* doc, QWidget* parent = nullptr);

    void setSelection(int gi, int ei);

private slots:
    void onDocumentChanged();
    void onTextEditingFinished();
    void onAlignXChanged(int index);
    void onAlignYChanged(int index);
    void onEllipsizeChanged(int index);
    void onWrapModeChanged(int index);
    void onAutoScaleToggled(bool checked);
    void onFontFamilyChanged(const QFont& font);
    void onFontSizeChanged(double value);
    void onFontWeightChanged(int index);
    void onFontItalicToggled(bool checked);
    void onTextColorChanged(double r, double g, double b, double a);

private:
    SceneDocument*          m_doc;
    int                     m_gi{-1};
    int                     m_ei{-1};
    bool                    m_updating{false};

    ElementPropertiesWidget* m_elementProps;

    // Text Content
    QLineEdit*       m_textEdit;
    QComboBox*       m_alignXCombo;
    QComboBox*       m_alignYCombo;
    QComboBox*       m_ellipsizeCombo;
    QComboBox*       m_wrapModeCombo;
    QCheckBox*       m_autoScaleCheck;

    // Font
    QFontComboBox*   m_fontFamilyCombo;
    QDoubleSpinBox*  m_fontSizeSpin;
    QComboBox*       m_fontWeightCombo;
    QCheckBox*       m_fontItalicCheck;
    ColorPickerButton* m_textColorBtn;
};
