#include "TextPropertiesWidget.h"
#include "ElementPropertiesWidget.h"
#include "ui/widgets/ColorPickerButton.h"

#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "engine/scene.h"
#include "engine/graphic.h"
#include "engine/element.h"
#include "engine/types.hpp"

#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QFontComboBox>
#include <QDoubleSpinBox>

// Merge tags for text-specific numeric fields
namespace TextMergeTag {
    enum : int {
        FontSize = 2000,
    };
}

TextPropertiesWidget::TextPropertiesWidget(SceneDocument* doc, QWidget* parent)
    : QWidget(parent)
    , m_doc(doc)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    // 1) Common element properties
    m_elementProps = new ElementPropertiesWidget(m_doc, this);
    layout->addWidget(m_elementProps);

    // 2) Text Content
    auto* textBox  = new QGroupBox("Text Content", this);
    auto* textForm = new QFormLayout(textBox);

    m_textEdit = new QLineEdit(textBox);
    textForm->addRow("Text:", m_textEdit);

    m_alignXCombo = new QComboBox(textBox);
    m_alignXCombo->addItem("Near",   QVariant::fromValue(static_cast<int>(Alignment::Near)));
    m_alignXCombo->addItem("Center", QVariant::fromValue(static_cast<int>(Alignment::Center)));
    m_alignXCombo->addItem("Far",    QVariant::fromValue(static_cast<int>(Alignment::Far)));
    textForm->addRow("Align X:", m_alignXCombo);

    m_alignYCombo = new QComboBox(textBox);
    m_alignYCombo->addItem("Near",   QVariant::fromValue(static_cast<int>(Alignment::Near)));
    m_alignYCombo->addItem("Center", QVariant::fromValue(static_cast<int>(Alignment::Center)));
    m_alignYCombo->addItem("Far",    QVariant::fromValue(static_cast<int>(Alignment::Far)));
    textForm->addRow("Align Y:", m_alignYCombo);

    m_ellipsizeCombo = new QComboBox(textBox);
    m_ellipsizeCombo->addItem("None",   QVariant::fromValue(static_cast<int>(Ellipsize::None)));
    m_ellipsizeCombo->addItem("Start",  QVariant::fromValue(static_cast<int>(Ellipsize::Start)));
    m_ellipsizeCombo->addItem("Middle", QVariant::fromValue(static_cast<int>(Ellipsize::Middle)));
    m_ellipsizeCombo->addItem("End",    QVariant::fromValue(static_cast<int>(Ellipsize::End)));
    textForm->addRow("Ellipsize:", m_ellipsizeCombo);

    m_wrapModeCombo = new QComboBox(textBox);
    m_wrapModeCombo->addItem("Word",      QVariant::fromValue(static_cast<int>(WrapMode::Word)));
    m_wrapModeCombo->addItem("Char",      QVariant::fromValue(static_cast<int>(WrapMode::Char)));
    m_wrapModeCombo->addItem("Word Char", QVariant::fromValue(static_cast<int>(WrapMode::WordChar)));
    textForm->addRow("Wrap Mode:", m_wrapModeCombo);

    m_autoScaleCheck = new QCheckBox("Auto Scale", textBox);
    textForm->addRow("", m_autoScaleCheck);

    layout->addWidget(textBox);

    // 3) Font
    auto* fontBox  = new QGroupBox("Font", this);
    auto* fontForm = new QFormLayout(fontBox);

    m_fontFamilyCombo = new QFontComboBox(fontBox);
    m_fontFamilyCombo->setMinimumContentsLength(10);
    m_fontFamilyCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_fontFamilyCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fontForm->addRow("Family:", m_fontFamilyCombo);

    m_fontSizeSpin = new QDoubleSpinBox(fontBox);
    m_fontSizeSpin->setRange(1.0, 400.0);
    m_fontSizeSpin->setDecimals(1);
    m_fontSizeSpin->setSingleStep(1.0);
    m_fontSizeSpin->setSuffix(" pt");
    fontForm->addRow("Size:", m_fontSizeSpin);

    m_fontWeightCombo = new QComboBox(fontBox);
    m_fontWeightCombo->addItem("Normal",     QVariant::fromValue(static_cast<int>(FontWeight::Normal)));
    m_fontWeightCombo->addItem("Medium",     QVariant::fromValue(static_cast<int>(FontWeight::Medium)));
    m_fontWeightCombo->addItem("Bold",       QVariant::fromValue(static_cast<int>(FontWeight::Bold)));
    m_fontWeightCombo->addItem("Semi Bold",  QVariant::fromValue(static_cast<int>(FontWeight::SemiBold)));
    m_fontWeightCombo->addItem("Ultra Bold", QVariant::fromValue(static_cast<int>(FontWeight::UltraBold)));
    m_fontWeightCombo->addItem("Light",      QVariant::fromValue(static_cast<int>(FontWeight::Light)));
    m_fontWeightCombo->addItem("Semi Light", QVariant::fromValue(static_cast<int>(FontWeight::SemiLight)));
    m_fontWeightCombo->addItem("Ultra Light",QVariant::fromValue(static_cast<int>(FontWeight::UltraLight)));
    m_fontWeightCombo->addItem("Thin",       QVariant::fromValue(static_cast<int>(FontWeight::Thin)));
    m_fontWeightCombo->addItem("Heavy",      QVariant::fromValue(static_cast<int>(FontWeight::Heavy)));
    m_fontWeightCombo->addItem("Ultra Heavy",QVariant::fromValue(static_cast<int>(FontWeight::UltraHeavy)));
    m_fontWeightCombo->addItem("Book",       QVariant::fromValue(static_cast<int>(FontWeight::Book)));
    fontForm->addRow("Weight:", m_fontWeightCombo);

    m_fontItalicCheck = new QCheckBox("Italic", fontBox);
    fontForm->addRow("", m_fontItalicCheck);

    m_textColorBtn = new ColorPickerButton(fontBox);
    fontForm->addRow("Color:", m_textColorBtn);

    layout->addWidget(fontBox);
    layout->addStretch();

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_textEdit, &QLineEdit::editingFinished,
            this, &TextPropertiesWidget::onTextEditingFinished);

    connect(m_alignXCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TextPropertiesWidget::onAlignXChanged);
    connect(m_alignYCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TextPropertiesWidget::onAlignYChanged);
    connect(m_ellipsizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TextPropertiesWidget::onEllipsizeChanged);
    connect(m_wrapModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TextPropertiesWidget::onWrapModeChanged);
    connect(m_autoScaleCheck, &QCheckBox::toggled,
            this, &TextPropertiesWidget::onAutoScaleToggled);

    connect(m_fontFamilyCombo, &QFontComboBox::currentFontChanged,
            this, &TextPropertiesWidget::onFontFamilyChanged);
    connect(m_fontSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TextPropertiesWidget::onFontSizeChanged);
    connect(m_fontWeightCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TextPropertiesWidget::onFontWeightChanged);
    connect(m_fontItalicCheck, &QCheckBox::toggled,
            this, &TextPropertiesWidget::onFontItalicToggled);
    connect(m_textColorBtn, &ColorPickerButton::colorChanged,
            this, &TextPropertiesWidget::onTextColorChanged);

    connect(m_doc, &SceneDocument::documentChanged,
            this, &TextPropertiesWidget::onDocumentChanged);
}

void TextPropertiesWidget::setSelection(int gi, int ei)
{
    m_gi = gi;
    m_ei = ei;
    m_elementProps->setSelection(gi, ei);
    onDocumentChanged();
}

void TextPropertiesWidget::onDocumentChanged()
{
    if (m_updating) return;
    if (m_gi < 0 || m_ei < 0) return;

    const Scene& s = m_doc->scene();
    if (m_gi >= static_cast<int>(s.graphics.size())) return;
    const Graphic& g = s.graphics[m_gi];
    if (m_ei >= static_cast<int>(g.elements.size())) return;

    m_updating = true;
    const Element& el = g.elements[m_ei];

    m_textEdit->setText(QString::fromStdString(el.text));

    // Align X
    int axVal = static_cast<int>(el.textAlignX);
    for (int i = 0; i < m_alignXCombo->count(); ++i) {
        if (m_alignXCombo->itemData(i).toInt() == axVal) {
            m_alignXCombo->setCurrentIndex(i); break;
        }
    }

    // Align Y
    int ayVal = static_cast<int>(el.textAlignY);
    for (int i = 0; i < m_alignYCombo->count(); ++i) {
        if (m_alignYCombo->itemData(i).toInt() == ayVal) {
            m_alignYCombo->setCurrentIndex(i); break;
        }
    }

    // Ellipsize
    int elVal = static_cast<int>(el.ellipsize);
    for (int i = 0; i < m_ellipsizeCombo->count(); ++i) {
        if (m_ellipsizeCombo->itemData(i).toInt() == elVal) {
            m_ellipsizeCombo->setCurrentIndex(i); break;
        }
    }

    // Wrap mode
    int wmVal = static_cast<int>(el.wrapMode);
    for (int i = 0; i < m_wrapModeCombo->count(); ++i) {
        if (m_wrapModeCombo->itemData(i).toInt() == wmVal) {
            m_wrapModeCombo->setCurrentIndex(i); break;
        }
    }

    m_autoScaleCheck->setChecked(el.autoScale);

    // Font
    QFont f(QString::fromStdString(el.font.family));
    m_fontFamilyCombo->setCurrentFont(f);
    m_fontSizeSpin->setValue(static_cast<double>(el.font.size));

    // Font weight
    int fwVal = static_cast<int>(el.font.weight);
    for (int i = 0; i < m_fontWeightCombo->count(); ++i) {
        if (m_fontWeightCombo->itemData(i).toInt() == fwVal) {
            m_fontWeightCombo->setCurrentIndex(i); break;
        }
    }

    m_fontItalicCheck->setChecked(el.font.isItalic);

    // Text color: stored as fill (Solid paint for text elements)
    if (el.fill.type == Paint::Type::Solid) {
        m_textColorBtn->setColor(el.fill.params[0], el.fill.params[1],
                                  el.fill.params[2], el.fill.params[3]);
    } else {
        m_textColorBtn->setColor(1.0, 1.0, 1.0, 1.0);
    }

    m_updating = false;
}

void TextPropertiesWidget::onTextEditingFinished()
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    std::string newText = m_textEdit->text().toStdString();
    const Scene& s = m_doc->scene();
    if (m_gi >= static_cast<int>(s.graphics.size())) return;
    if (m_ei >= static_cast<int>(s.graphics[m_gi].elements.size())) return;
    if (s.graphics[m_gi].elements[m_ei].text == newText) return;

    m_doc->undoStack()->push(new SetElementFieldCmd<std::string>(
        m_doc, m_gi, m_ei, newText,
        [](Element& e) -> std::string& { return e.text; },
        "text"
    ));
}

void TextPropertiesWidget::onAlignXChanged(int /*index*/)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    Alignment val = static_cast<Alignment>(m_alignXCombo->currentData().toInt());
    m_doc->undoStack()->push(new SetElementFieldCmd<Alignment>(
        m_doc, m_gi, m_ei, val,
        [](Element& e) -> Alignment& { return e.textAlignX; },
        "textAlignX"
    ));
}

void TextPropertiesWidget::onAlignYChanged(int /*index*/)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    Alignment val = static_cast<Alignment>(m_alignYCombo->currentData().toInt());
    m_doc->undoStack()->push(new SetElementFieldCmd<Alignment>(
        m_doc, m_gi, m_ei, val,
        [](Element& e) -> Alignment& { return e.textAlignY; },
        "textAlignY"
    ));
}

void TextPropertiesWidget::onEllipsizeChanged(int /*index*/)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    Ellipsize val = static_cast<Ellipsize>(m_ellipsizeCombo->currentData().toInt());
    m_doc->undoStack()->push(new SetElementFieldCmd<Ellipsize>(
        m_doc, m_gi, m_ei, val,
        [](Element& e) -> Ellipsize& { return e.ellipsize; },
        "ellipsize"
    ));
}

void TextPropertiesWidget::onWrapModeChanged(int /*index*/)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    WrapMode val = static_cast<WrapMode>(m_wrapModeCombo->currentData().toInt());
    m_doc->undoStack()->push(new SetElementFieldCmd<WrapMode>(
        m_doc, m_gi, m_ei, val,
        [](Element& e) -> WrapMode& { return e.wrapMode; },
        "wrapMode"
    ));
}

void TextPropertiesWidget::onAutoScaleToggled(bool checked)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_gi, m_ei, checked,
        [](Element& e) -> bool& { return e.autoScale; },
        "autoScale"
    ));
}

void TextPropertiesWidget::onFontFamilyChanged(const QFont& font)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    std::string family = font.family().toStdString();
    m_doc->undoStack()->push(new SetElementFieldCmd<std::string>(
        m_doc, m_gi, m_ei, family,
        [](Element& e) -> std::string& { return e.font.family; },
        "font.family"
    ));
}

void TextPropertiesWidget::onFontSizeChanged(double value)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_gi, m_ei, static_cast<float>(value),
        [](Element& e) -> float& { return e.font.size; },
        "font.size", TextMergeTag::FontSize
    ));
}

void TextPropertiesWidget::onFontWeightChanged(int /*index*/)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    FontWeight val = static_cast<FontWeight>(m_fontWeightCombo->currentData().toInt());
    m_doc->undoStack()->push(new SetElementFieldCmd<FontWeight>(
        m_doc, m_gi, m_ei, val,
        [](Element& e) -> FontWeight& { return e.font.weight; },
        "font.weight"
    ));
}

void TextPropertiesWidget::onFontItalicToggled(bool checked)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_gi, m_ei, checked,
        [](Element& e) -> bool& { return e.font.isItalic; },
        "font.isItalic"
    ));
}

void TextPropertiesWidget::onTextColorChanged(double r, double g, double b, double a)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    // Text color maps to element.fill as a Solid paint
    Paint solidColor = Paint::Solid(r, g, b, a);
    m_doc->undoStack()->push(new SetElementPaintCmd(
        m_doc, m_gi, m_ei,
        SetElementPaintCmd::Target::Fill,
        solidColor
    ));
}
