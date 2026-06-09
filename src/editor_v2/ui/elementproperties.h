#ifndef ELEMENTPROPERTIES_H
#define ELEMENTPROPERTIES_H

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#include <string>

class AnimationEditor;
class PaddingEditor;
class TransformEditor;
class SceneDocument;

class ElementProperties : public QWidget {
    Q_OBJECT

public:
    explicit ElementProperties(SceneDocument* doc, QWidget* parent = nullptr);

    void setSelection(const std::string& graphicId, const std::string& elementId);

signals:
    void elementIdChanged(const std::string& graphicId, const std::string& newElementId);

private slots:
    void onDocumentChanged();
    void onIdEditingFinished();
    void onZOrderChanged(int value);
    void onOpacityChanged(double value);
    void onXChanged(double value);
    void onYChanged(double value);
    void onWChanged(double value);
    void onHChanged(double value);
    void onRotChanged(double value);
    void onAnimInChanged();
    void onAnimOutChanged();
    void onMaskChanged(int index);
    void onParentChanged(int index);
    void onFitToChildrenToggled(bool checked);
    void onChildrenPaddingChanged(float top, float right, float bottom, float left);

private:
    void populateRefCombos();

    SceneDocument* m_doc;
    std::string m_graphicId;
    std::string m_elementId;
    bool m_updating{false};

    QLineEdit* idLineEdit;
    QSpinBox* zOrderSpinBox;
    QDoubleSpinBox* opacitySpinBox;
    QComboBox *maskComboBox, *parentComboBox;
    TransformEditor* transformEditor;
    AnimationEditor *animateIn, *animateOut;

    QGroupBox* m_fitChildrenBox;
    QCheckBox* m_fitChildrenCheck;
    PaddingEditor* m_paddingEditor;
};

#endif // ELEMENTPROPERTIES_H
