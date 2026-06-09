#include "fontproperties.h"
#include "engine/element.h"
#include "engine/graphic.h"
#include "engine/scene.h"
#include "icons.h"
#include "model/SceneDocument.h"
#include "model/UndoCommands.h"

#include <QAbstractItemView>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCompleter>
#include <QFileDialog>
#include <QFont>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

// Paints each font item in its own typeface, constructing QFont only when visible.
class FontItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override
    {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, idx);
        const QString family = idx.data(Qt::UserRole).toString();
        if (!family.isEmpty())
            o.font.setFamily(family);
        QStyledItemDelegate::paint(p, o, idx);
    }
};

static QGroupBox* makeGroup(const QString& title)
{
    auto* box = new QGroupBox(title);
    box->setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; }");
    return box;
}

static int weightToIndex(FontWeight w)
{
    // Must match combo items in order: Thin UltraLight Light Regular Medium SemiBold Bold UltraBold
    // Heavy UltraHeavy
    switch (static_cast<int>(w)) {
    case PANGO_WEIGHT_THIN:
        return 0;
    case PANGO_WEIGHT_ULTRALIGHT:
        return 1;
    case PANGO_WEIGHT_LIGHT:
        return 2;
    case PANGO_WEIGHT_NORMAL:
        return 3;
    case PANGO_WEIGHT_MEDIUM:
        return 4;
    case PANGO_WEIGHT_SEMIBOLD:
        return 5;
    case PANGO_WEIGHT_BOLD:
        return 6;
    case PANGO_WEIGHT_ULTRABOLD:
        return 7;
    case PANGO_WEIGHT_HEAVY:
        return 8;
    case PANGO_WEIGHT_ULTRAHEAVY:
        return 9;
    default:
        return 3;
    }
}

static FontWeight indexToWeight(int i)
{
    switch (i) {
    case 0:
        return FontWeight::Thin;
    case 1:
        return FontWeight::UltraLight;
    case 2:
        return FontWeight::Light;
    case 3:
        return FontWeight::Normal;
    case 4:
        return FontWeight::Medium;
    case 5:
        return FontWeight::SemiBold;
    case 6:
        return FontWeight::Bold;
    case 7:
        return FontWeight::UltraBold;
    case 8:
        return FontWeight::Heavy;
    case 9:
        return FontWeight::UltraHeavy;
    default:
        return FontWeight::Normal;
    }
}

FontProperties::FontProperties(SceneDocument* doc, QWidget* parent) : QWidget(parent), m_doc(doc)
{
    auto* outerLayout = new QVBoxLayout(this);

    // Placeholder shown when no Text element is selected
    m_placeholder = new QLabel("Select a Text element to configure font settings.");
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);
    m_placeholder->setStyleSheet("color: #888; padding: 20px;");
    outerLayout->addWidget(m_placeholder);

    // All font controls in one widget, hidden when no Text element is selected
    m_controls = new QWidget;
    auto* innerLayout = new QVBoxLayout(m_controls);
    innerLayout->setContentsMargins(0, 0, 0, 0);

    // ── Text content ─────────────────────────────────────────────────────────
    auto* textBox = makeGroup("Text");
    auto* textLayout = new QVBoxLayout(textBox);
    textLayout->setContentsMargins(0, 0, 0, 0);
    m_textEdit = new QPlainTextEdit;
    m_textEdit->setFixedHeight(80);
    textLayout->addWidget(m_textEdit);
    innerLayout->addWidget(textBox);

    // ── Font family & size ────────────────────────────────────────────────────
    auto* familyBox = makeGroup("Font");
    auto* familyForm = new QFormLayout(familyBox);
    familyForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_fontFamily = new QComboBox();
    m_fontFamily->setMaxVisibleItems(20);
    loadFonts();

    m_fontFamily->setEditable(true);
    m_fontFamily->setInsertPolicy(QComboBox::NoInsert);
    m_fontFamily->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_fontFamily->setMinimumContentsLength(18);
    m_fontFamily->view()->setTextElideMode(Qt::ElideRight);
    if (auto* c = m_fontFamily->completer()) {
        c->setFilterMode(Qt::MatchContains);
        c->setCaseSensitivity(Qt::CaseInsensitive);
    }

    familyForm->addRow("Font", m_fontFamily);

    m_fontWeight = new QComboBox();
    for (const char* w : {"Thin", "Ultra Light", "Light", "Regular", "Medium", "Semi Bold", "Bold",
                          "Ultra Bold", "Heavy", "Ultra Heavy"})
        m_fontWeight->addItem(w);
    familyForm->addRow("Weight", m_fontWeight);

    m_fontSize = new QDoubleSpinBox;
    m_fontSize->setRange(1.0, 9999.0);
    m_fontSize->setDecimals(1);
    m_fontSize->setSuffix(" px");
    m_fontSize->setSingleStep(1.0);
    familyForm->addRow("Size", m_fontSize);
    setFieldIcon(familyForm, m_fontSize, fa::fa_text_height);

    // ── Layout ────────────────────────────────────────────────────────────────
    QWidget* layoutToolbar = new QWidget();
    QHBoxLayout* toolLayout = new QHBoxLayout(layoutToolbar);
    toolLayout->setContentsMargins(2, 2, 2, 2);
    toolLayout->setSpacing(4);
    toolLayout->setAlignment(Qt::AlignLeft);

    { // H align
        QButtonGroup* group = new QButtonGroup(layoutToolbar);
        group->setExclusive(true);

        m_alignLeft = createToolButton(fa::fa_align_left, "Align Left", group);
        toolLayout->addWidget(m_alignLeft);

        m_alignCenter = createToolButton(fa::fa_align_center, "Align Center", group);
        toolLayout->addWidget(m_alignCenter);

        m_alignJustify = createToolButton(fa::fa_align_justify, "Align Justify", group);
        toolLayout->addWidget(m_alignJustify);

        m_alignRight = createToolButton(fa::fa_align_right, "Align Right", group);
        toolLayout->addWidget(m_alignRight);

        toolLayout->addWidget(createVDivider());
    }
    { // V align
        QButtonGroup* group = new QButtonGroup(layoutToolbar);
        group->setExclusive(true);

        m_alignTop = createToolButton(fa::fa_arrow_up, "Align Top", group);
        toolLayout->addWidget(m_alignTop);

        m_alignMiddle = createToolButton(fa::fa_arrows_left_right, "Align Middle", group);
        toolLayout->addWidget(m_alignMiddle);

        m_alignBottom = createToolButton(fa::fa_arrow_down, "Align Bottom", group);
        toolLayout->addWidget(m_alignBottom);
    }
    familyForm->addRow("Layout", layoutToolbar);

    // ── Appearance ──────────────────────────────────────────────────────────────
    QWidget* appearanceToolbar = new QWidget();
    QHBoxLayout* toolAppearance = new QHBoxLayout(appearanceToolbar);
    toolAppearance->setContentsMargins(2, 2, 2, 2);
    toolAppearance->setSpacing(4);
    toolAppearance->setAlignment(Qt::AlignLeft);

    { // Options
        m_italicOption = createToolButton(fa::fa_italic, "Italic");
        toolAppearance->addWidget(m_italicOption);

        m_underlineOption = createToolButton(fa::fa_underline, "Underline");
        toolAppearance->addWidget(m_underlineOption);

        m_strikethroughOption = createToolButton(fa::fa_strikethrough, "Strikethrough");
        toolAppearance->addWidget(m_strikethroughOption);
    }
    familyForm->addRow("Appearance", appearanceToolbar);

    familyForm->addWidget(createHDivider());

    m_autoScale = new QCheckBox("Auto scale to fit bounds");
    familyForm->addRow("", m_autoScale);

    m_ellipsize = new QComboBox;
    m_ellipsize->addItems({"None", "Start", "Middle", "End"});
    familyForm->addRow("Ellipsize", m_ellipsize);

    m_wrap = new QComboBox;
    m_wrap->addItems({"Word", "Character", "Word + Char"});
    familyForm->addRow("Wrap", m_wrap);

    innerLayout->addWidget(familyBox);
    innerLayout->addStretch(1);

    outerLayout->addWidget(m_controls);
    outerLayout->addStretch(1);

    m_controls->setVisible(false);

    // Connections
    connect(m_fontFamily, QOverload<int>::of(&QComboBox::activated), this,
            &FontProperties::onFontFamilyChanged);
    connect(m_fontSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &FontProperties::onFontSizeChanged);
    connect(m_fontWeight, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &FontProperties::onFontWeightChanged);
    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &FontProperties::onTextChanged);
    connect(m_autoScale, &QCheckBox::toggled, this, &FontProperties::onAutoScaleToggled);
    connect(m_ellipsize, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &FontProperties::onEllipsizeChanged);
    connect(m_wrap, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &FontProperties::onWrapChanged);

    connect(m_italicOption, &QToolButton::toggled, this, &FontProperties::onFontItalicToggled);
    connect(m_underlineOption, &QToolButton::toggled, this, &FontProperties::onUnderlineToggled);
    connect(m_strikethroughOption, &QToolButton::toggled, this,
            &FontProperties::onStrikethroughToggled);

    connect(m_alignLeft, &QToolButton::clicked, this, [this] { onAlignXChanged(0); });
    connect(m_alignCenter, &QToolButton::clicked, this, [this] { onAlignXChanged(1); });
    connect(m_alignJustify, &QToolButton::clicked, this, [this] { onAlignXChanged(2); });
    connect(m_alignRight, &QToolButton::clicked, this, [this] { onAlignXChanged(3); });

    connect(m_alignTop, &QToolButton::clicked, this, [this] { onAlignYChanged(0); });
    connect(m_alignMiddle, &QToolButton::clicked, this, [this] { onAlignYChanged(1); });
    connect(m_alignBottom, &QToolButton::clicked, this, [this] { onAlignYChanged(2); });

    connect(m_doc, &SceneDocument::documentChanged, this, &FontProperties::onDocumentChanged);
}

void FontProperties::loadFonts()
{
    auto* model = new QStandardItemModel(this);

    // First item: browse for font file
    auto* browseItem = new QStandardItem("Open font from file...");
    browseItem->setData(QString(), Qt::UserRole); // no family name
    model->appendRow(browseItem);

    // System fonts — no QFont constructed here; delegate handles it lazily
    const QStringList families = QFontDatabase::families();
    for (const QString& family : families) {
        auto* item = new QStandardItem(family);
        item->setData(family, Qt::UserRole);
        model->appendRow(item);
    }

    m_fontFamily->setModel(model);
    m_fontFamily->view()->setItemDelegate(new FontItemDelegate(m_fontFamily));
}

void FontProperties::browseForFont()
{
    const QString path =
        QFileDialog::getOpenFileName(this, "Open Font File", QString(),
                                     "Font files (*.ttf *.otf *.woff *.woff2);;All files (*)");
    if (path.isEmpty())
        return;

    int id = QFontDatabase::addApplicationFont(path);
    if (id < 0)
        return;

    const QStringList loaded = QFontDatabase::applicationFontFamilies(id);
    if (loaded.isEmpty())
        return;

    const QString family = loaded.first();

    // Check if already in the model (from a previous load)
    auto* model = qobject_cast<QStandardItemModel*>(m_fontFamily->model());
    for (int i = 1; i < model->rowCount(); ++i) {
        if (model->item(i)->data(Qt::UserRole).toString() == family) {
            m_fontFamily->setCurrentIndex(i);
            return;
        }
    }

    // Insert new entry right after the "browse" item
    auto* item = new QStandardItem(family + " (file)");
    item->setData(family, Qt::UserRole);
    QFont f(family, 11);
    item->setData(f, Qt::FontRole);
    model->insertRow(1, item);
    m_fontFamily->setCurrentIndex(1);
}

QToolButton* FontProperties::createToolButton(int icon, const QString& tooltip, QButtonGroup* group)
{
    auto* btn = new QToolButton();
    btn->setCheckable(true);
    btn->setIcon(qta()->icon(fa::fa_solid, icon));
    btn->setIconSize(QSize(18, 18));
    btn->setToolTip(tooltip);
    btn->setStyleSheet("QToolButton:checked { "
                       "    background-color: #222; "
                       "    font-weight: bold; "
                       "} ");
    if (group)
        group->addButton(btn);
    return btn;
}

QFrame* FontProperties::createVDivider()
{
    QFrame* frame = new QFrame();
    frame->setFrameShape(QFrame::VLine);
    frame->setFrameShadow(QFrame::Sunken);
    return frame;
}

QFrame* FontProperties::createHDivider()
{
    QFrame* frame = new QFrame();
    frame->setFrameShape(QFrame::HLine);
    frame->setFrameShadow(QFrame::Sunken);
    return frame;
}

void FontProperties::setFieldIcon(QFormLayout* form, QWidget* field, int icon)
{
    ((QLabel*)form->labelForField(field))
        ->setPixmap(qta()->icon(fa::fa_solid, icon).pixmap(18, 18));
}

void FontProperties::setSelection(const std::string& graphicId, const std::string& elementId)
{
    m_graphicId = graphicId;
    m_elementId = elementId;
    onDocumentChanged();
}

void FontProperties::onDocumentChanged()
{
    if (m_updating)
        return;

    if (m_graphicId.empty() || m_elementId.empty()) {
        m_placeholder->setVisible(true);
        m_controls->setVisible(false);
        return;
    }

    Scene& s = m_doc->scene();
    const Element* el = nullptr;
    try {
        el = &s.GetById(m_graphicId).GetById(m_elementId);
    } catch (const std::runtime_error&) {
    }

    if (!el || el->type != ElementType::Text) {
        m_placeholder->setVisible(true);
        m_controls->setVisible(false);
        return;
    }

    m_placeholder->setVisible(false);
    m_controls->setVisible(true);

    m_updating = true;

    // Font family: find by UserRole data
    const QString family = QString::fromStdString(el->font.family);
    auto* model = qobject_cast<QStandardItemModel*>(m_fontFamily->model());
    int idx = -1;
    for (int i = 1; i < model->rowCount(); ++i) {
        if (model->item(i)->data(Qt::UserRole).toString() == family) {
            idx = i;
            break;
        }
    }
    m_fontFamily->setCurrentIndex(idx >= 0 ? idx : 0);

    m_fontSize->setValue(static_cast<double>(el->font.size));
    m_fontWeight->setCurrentIndex(weightToIndex(el->font.weight));
    m_italicOption->setChecked(el->font.isItalic);
    m_underlineOption->setChecked(el->font.isUnderline);
    m_strikethroughOption->setChecked(el->font.isStrikethrough);

    m_textEdit->setPlainText(QString::fromStdString(el->text));

    QTextCursor cursor(m_textEdit->document());
    cursor.setPosition(m_savedTextCursor);
    m_textEdit->setTextCursor(cursor);

    m_autoScale->setChecked(el->autoScale);

    m_alignLeft->setChecked(el->textAlignX == HorizontalAlignment::Left);
    m_alignCenter->setChecked(el->textAlignX == HorizontalAlignment::Center);
    m_alignJustify->setChecked(el->textAlignX == HorizontalAlignment::Justify);
    m_alignRight->setChecked(el->textAlignX == HorizontalAlignment::Right);

    m_alignTop->setChecked(el->textAlignY == VerticalAlignment::Top);
    m_alignMiddle->setChecked(el->textAlignY == VerticalAlignment::Middle);
    m_alignBottom->setChecked(el->textAlignY == VerticalAlignment::Bottom);

    switch (el->ellipsize) {
    case Ellipsize::Start:
        m_ellipsize->setCurrentIndex(1);
        break;
    case Ellipsize::Middle:
        m_ellipsize->setCurrentIndex(2);
        break;
    case Ellipsize::End:
        m_ellipsize->setCurrentIndex(3);
        break;
    default:
        m_ellipsize->setCurrentIndex(0);
        break;
    }

    switch (el->wrapMode) {
    case WrapMode::Char:
        m_wrap->setCurrentIndex(1);
        break;
    case WrapMode::WordChar:
        m_wrap->setCurrentIndex(2);
        break;
    default:
        m_wrap->setCurrentIndex(0);
        break;
    }

    m_updating = false;
}

void FontProperties::onFontFamilyChanged(int index)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    if (index == 0) {
        // Browse for font file then re-evaluate
        browseForFont();
        return;
    }
    auto* model = qobject_cast<QStandardItemModel*>(m_fontFamily->model());
    const std::string family = model->item(index)->data(Qt::UserRole).toString().toStdString();
    m_doc->undoStack()->push(new SetElementFieldCmd<std::string>(
        m_doc, m_graphicId, m_elementId, family,
        [](Element& e) -> std::string& { return e.font.family; }, "font_family"));
}

void FontProperties::onFontSizeChanged(double value)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_graphicId, m_elementId, static_cast<float>(value),
        [](Element& e) -> float& { return e.font.size; }, "font_size",
        ElemMergeTag::StrokeW + 10 // reuse a unique merge tag
        ));
}

void FontProperties::onFontWeightChanged(int index)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    FontWeight w = indexToWeight(index);
    m_doc->undoStack()->push(new SetElementFieldCmd<FontWeight>(
        m_doc, m_graphicId, m_elementId, w, [](Element& e) -> FontWeight& { return e.font.weight; },
        "font_weight"));
}

void FontProperties::onFontItalicToggled(bool checked)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_graphicId, m_elementId, checked,
        [](Element& e) -> bool& { return e.font.isItalic; }, "font_italic"));
}

void FontProperties::onUnderlineToggled(bool checked)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_graphicId, m_elementId, checked,
        [](Element& e) -> bool& { return e.font.isUnderline; }, "font_underline"));
}

void FontProperties::onStrikethroughToggled(bool checked)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_graphicId, m_elementId, checked,
        [](Element& e) -> bool& { return e.font.isStrikethrough; }, "font_strikethrough"));
}

void FontProperties::onTextChanged()
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    const std::string text = m_textEdit->toPlainText().toStdString();
    m_savedTextCursor = m_textEdit->textCursor().position();
    m_doc->undoStack()->push(new SetElementFieldCmd<std::string>(
        m_doc, m_graphicId, m_elementId, text, [](Element& e) -> std::string& { return e.text; },
        "text"));
}

void FontProperties::onAutoScaleToggled(bool checked)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_graphicId, m_elementId, checked, [](Element& e) -> bool& { return e.autoScale; },
        "auto_scale"));
}

void FontProperties::onAlignXChanged(int index)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    const HorizontalAlignment a = static_cast<HorizontalAlignment>(index);
    m_doc->undoStack()->push(new SetElementFieldCmd<HorizontalAlignment>(
        m_doc, m_graphicId, m_elementId, a,
        [](Element& e) -> HorizontalAlignment& { return e.textAlignX; }, "text_align_x"));
}

void FontProperties::onAlignYChanged(int index)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    const VerticalAlignment a = static_cast<VerticalAlignment>(index);
    m_doc->undoStack()->push(new SetElementFieldCmd<VerticalAlignment>(
        m_doc, m_graphicId, m_elementId, a,
        [](Element& e) -> VerticalAlignment& { return e.textAlignY; }, "text_align_y"));
}

void FontProperties::onEllipsizeChanged(int index)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    static const Ellipsize kMap[] = {Ellipsize::None, Ellipsize::Start, Ellipsize::Middle,
                                     Ellipsize::End};
    const Ellipsize e = kMap[index];
    m_doc->undoStack()->push(new SetElementFieldCmd<Ellipsize>(
        m_doc, m_graphicId, m_elementId, e, [](Element& el) -> Ellipsize& { return el.ellipsize; },
        "ellipsize"));
}

void FontProperties::onWrapChanged(int index)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    static const WrapMode kMap[] = {WrapMode::Word, WrapMode::Char, WrapMode::WordChar};
    const WrapMode wm = kMap[index];
    m_doc->undoStack()->push(new SetElementFieldCmd<WrapMode>(
        m_doc, m_graphicId, m_elementId, wm, [](Element& e) -> WrapMode& { return e.wrapMode; },
        "wrap"));
}
