#pragma once

#include <QWidget>

class SceneDocument;
class QLineEdit;
class QSpinBox;
class QLabel;

class GraphicPropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit GraphicPropertiesWidget(SceneDocument* doc, QWidget* parent = nullptr);

    void setSelection(int graphicIndex);

private slots:
    void onDocumentChanged();
    void onIdEditingFinished();
    void onZOrderChanged(int value);

private:
    SceneDocument* m_doc;
    int            m_gi{-1};
    QLineEdit*     m_idEdit;
    QSpinBox*      m_zOrderSpin;
    QLabel*        m_elementCountLabel;
    bool           m_updating{false};
};
