#ifndef ELEMENTPROPERTIES_H
#define ELEMENTPROPERTIES_H

#include <string>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>

class AnimationEditor;
class TransformEditor;
class SceneDocument;

class ElementProperties : public QWidget
{
    Q_OBJECT

public:
    explicit ElementProperties(SceneDocument* doc, QWidget *parent = nullptr);

    void setSelection(const std::string& graphicId, const std::string& elementId);

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

private:
    void populateRefCombos();

    SceneDocument* m_doc;
    std::string m_graphicId;
    std::string m_elementId;
    bool m_updating{false};

    QLineEdit *idLineEdit;
    QSpinBox *zOrderSpinBox;
    QDoubleSpinBox *opacitySpinBox;
    QComboBox *maskComboBox, *parentComboBox;
    TransformEditor *transformEditor;
    AnimationEditor *animateIn, *animateOut;
};

#endif // ELEMENTPROPERTIES_H
