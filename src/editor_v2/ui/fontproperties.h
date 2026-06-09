#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QWidget>
#include <string>

class SceneDocument;

class FontProperties : public QWidget {
    Q_OBJECT
public:
    explicit FontProperties(SceneDocument* doc, QWidget* parent = nullptr);

    void setSelection(const std::string& graphicId, const std::string& elementId);

private slots:
    void onDocumentChanged();
    void onFontFamilyChanged(int index);
    void onFontSizeChanged(double value);
    void onFontWeightChanged(int index);
    void onTextChanged();
    void onFontItalicToggled(bool checked);
    void onUnderlineToggled(bool checked);
    void onStrikethroughToggled(bool checked);
    void onAutoScaleToggled(bool checked);
    void onAlignXChanged(int index);
    void onAlignYChanged(int index);
    void onEllipsizeChanged(int index);
    void onWrapChanged(int index);

private:
    void loadFonts();
    void browseForFont();

    SceneDocument* m_doc;
    std::string m_graphicId;
    std::string m_elementId;
    bool m_updating{false};

    QLabel* m_placeholder;
    QWidget* m_controls;

    QComboBox* m_fontFamily;
    QDoubleSpinBox* m_fontSize;
    QComboBox* m_fontWeight;
    QPlainTextEdit* m_textEdit;
    QCheckBox* m_autoScale;
    QComboBox* m_ellipsize;
    QComboBox* m_wrap;

    QToolButton* m_alignLeft;
    QToolButton* m_alignCenter;
    QToolButton* m_alignJustify;
    QToolButton* m_alignRight;
    QToolButton* m_alignTop;
    QToolButton* m_alignMiddle;
    QToolButton* m_alignBottom;

    QToolButton* m_italicOption;
    QToolButton* m_underlineOption;
    QToolButton* m_strikethroughOption;

    QToolButton* createToolButton(int icon, const QString& tooltip, QButtonGroup* group = nullptr);
    QFrame* createVDivider();
    QFrame* createHDivider();
    void setFieldIcon(QFormLayout* form, QWidget* field, int icon);

    int m_savedTextCursor;
};
