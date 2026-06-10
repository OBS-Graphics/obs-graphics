#include "GraphicTimingEditor.h"
#include "AnimationTimingEditor.h"
#include "ScrubRuler.h"
#include "model/SceneDocument.h"
#include "engine/element.h"
#include "engine/graphic.h"

#include <QTabBar>
#include <QDoubleSpinBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <cmath>

static constexpr int kNameColWidth = 120;

GraphicTimingEditor::GraphicTimingEditor(SceneDocument* doc, QWidget* parent)
    : QWidget{parent}, m_doc{doc}
{
    // --- transport button (play/stop toggle) ---
    m_playBtn = new QPushButton("▶");
    m_playBtn->setFixedSize(26, 26);

    m_speedSlider = new QSlider(Qt::Horizontal);
    m_speedSlider->setRange(0, 100);
    m_speedSlider->setValue(100);
    m_speedSlider->setFixedWidth(80);
    m_speedSlider->setToolTip("Playback speed (0–1×)");

    m_timeLabel = new QLabel("0.00 s");
    m_timeLabel->setFixedWidth(52);
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // --- tab bar + spinbox ---
    m_tabBar = new QTabBar;
    m_tabBar->addTab("In");
    m_tabBar->addTab("Out");

    m_maxDurSpin = new QDoubleSpinBox;
    m_maxDurSpin->setRange(0.1, 60.0);
    m_maxDurSpin->setSingleStep(0.1);
    m_maxDurSpin->setDecimals(2);
    m_maxDurSpin->setSuffix(" s");
    m_maxDurSpin->setValue(1.0);
    m_maxDurSpin->setFixedWidth(80);

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 2, 4, 2);
    toolbar->setSpacing(4);
    toolbar->addWidget(m_tabBar);
    toolbar->addWidget(m_playBtn);
    toolbar->addWidget(m_timeLabel);
    toolbar->addWidget(new QLabel("Speed:"));
    toolbar->addWidget(m_speedSlider);
    toolbar->addStretch();
    toolbar->addWidget(new QLabel("Max. Duration:"));
    toolbar->addWidget(m_maxDurSpin);

    // --- scrub ruler + element grid ---
    m_ruler = new ScrubRuler;

    m_content = new QWidget;
    m_content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_grid = new QGridLayout(m_content);
    m_grid->setContentsMargins(4, 4, 4, 4);
    m_grid->setHorizontalSpacing(4);
    m_grid->setVerticalSpacing(2);
    m_grid->setColumnMinimumWidth(0, kNameColWidth);
    m_grid->setColumnStretch(1, 1);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_content);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(toolbar);
    root->addWidget(scroll);

    // --- timer ---
    m_playTimer = new QTimer(this);
    m_playTimer->setInterval(16);

    connect(m_tabBar,     &QTabBar::currentChanged,      this, &GraphicTimingEditor::onTabChanged);
    connect(m_maxDurSpin, &QDoubleSpinBox::valueChanged, this, &GraphicTimingEditor::onMaxDurationChanged);
    connect(m_playBtn,    &QPushButton::clicked,         this, [this] {
        m_playState == PlayState::Playing ? onStop() : onPlay();
    });
    connect(m_speedSlider, &QSlider::valueChanged, this, [this](int v) {
        m_playSpeed = v / 100.0f;
    });
    connect(m_playTimer,  &QTimer::timeout,              this, &GraphicTimingEditor::onTick);
    connect(m_ruler,      &ScrubRuler::scrubChanged,     this, &GraphicTimingEditor::setScrubTimeOnAll);
}

// ── public ──────────────────────────────────────────────────────────────────

void GraphicTimingEditor::loadGraphic(int graphicIndex)
{
    onStop();
    m_graphicIndex = graphicIndex;

    m_maxDurSpin->blockSignals(true);
    m_maxDurSpin->setValue(double(computeAutoMaxDuration()));
    m_maxDurSpin->blockSignals(false);

    m_ruler->setMaxDuration(float(m_maxDurSpin->value()));
    rebuild();
}

void GraphicTimingEditor::clear()
{
    onStop();
    m_graphicIndex = -1;
    rebuild();
}

// ── private slots ────────────────────────────────────────────────────────────

void GraphicTimingEditor::onTabChanged(int)
{
    onStop();
    if (m_graphicIndex < 0) return;

    m_maxDurSpin->blockSignals(true);
    m_maxDurSpin->setValue(double(computeAutoMaxDuration()));
    m_maxDurSpin->blockSignals(false);

    m_ruler->setMaxDuration(float(m_maxDurSpin->value()));
    rebuild(); // recreates lambdas with the correct isIn() capture
}

void GraphicTimingEditor::onMaxDurationChanged(double value)
{
    const float maxD = float(value);
    m_ruler->setMaxDuration(maxD);
    for (AnimationTimingEditor* ed : m_editors)
        ed->setMaxDuration(maxD);
}

void GraphicTimingEditor::onPlay()
{
    m_playState = PlayState::Playing;
    m_playBtn->setText("⏹");
    m_playTimer->start();
}

void GraphicTimingEditor::onStop()
{
    m_playState = PlayState::Stopped;
    m_playBtn->setText("▶");
    m_playTimer->stop();
    setScrubTimeOnAll(0.0f);
    emit previewStopped();
}

void GraphicTimingEditor::onTick()
{
    const float maxD = float(m_maxDurSpin->value());
    m_scrubTime += m_playTimer->interval() / 1000.0f * m_playSpeed;
    if (m_scrubTime >= maxD) {
        m_scrubTime = maxD;
        setScrubTimeOnAll(m_scrubTime);
        onStop();
        return;
    }
    setScrubTimeOnAll(m_scrubTime);
}

// ── private ──────────────────────────────────────────────────────────────────

bool GraphicTimingEditor::isIn() const
{
    return m_tabBar->currentIndex() == 0;
}

float GraphicTimingEditor::computeAutoMaxDuration() const
{
    if (m_graphicIndex < 0) return 1.0f;

    const Scene& s = m_doc->scene();
    if (m_graphicIndex >= (int)s.graphics.size()) return 1.0f;

    const Graphic& g = s.graphics[m_graphicIndex];
    float maxEnd = 0.1f;
    for (const Element& el : g.elements) {
        const AnimationDef& def = isIn() ? el.inAnimation : el.outAnimation;
        maxEnd = qMax(maxEnd, def.delay + def.duration);
    }
    return std::ceil(maxEnd * 2.0f) / 2.0f;
}

void GraphicTimingEditor::setScrubTimeOnAll(float t)
{
    m_scrubTime = t;
    m_ruler->setScrubTime(t);
    m_timeLabel->setText(QString("%1 s").arg(double(t), 0, 'f', 2));
    emit scrubTimeChanged(t);
}

void GraphicTimingEditor::rebuild()
{
    // Clear grid, preserving the ruler widget
    while (QLayoutItem* item = m_grid->takeAt(0)) {
        QWidget* w = item->widget();
        if (w && w != m_ruler)
            w->deleteLater();
        delete item;
    }
    m_editors.clear();

    // Ruler always at row 0
    m_grid->addWidget(m_ruler, 0, 1);

    if (m_graphicIndex < 0) return;

    const Scene& s = m_doc->scene();
    if (m_graphicIndex >= (int)s.graphics.size()) return;

    const Graphic& g   = s.graphics[m_graphicIndex];
    const float    maxD = float(m_maxDurSpin->value());
    const bool     in   = isIn();

    for (int i = 0; i < (int)g.elements.size(); ++i) {
        const Element& el = g.elements[i];

        auto* label = new QLabel(QString::fromStdString(el.id));
        label->setFixedWidth(kNameColWidth);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* editor = new AnimationTimingEditor;
        editor->setMaxDuration(maxD);
        editor->load(in ? el.inAnimation : el.outAnimation);

        const int row = i + 1;
        connect(editor, &AnimationTimingEditor::animationChanged,
                this, [this, i, in](AnimationType type, Easing easing, float delay, float duration) {
            emit animationChanged(i, in, type, easing, delay, duration);
        });

        m_grid->addWidget(label,  row, 0);
        m_grid->addWidget(editor, row, 1);
        m_editors.append(editor);
    }
}
