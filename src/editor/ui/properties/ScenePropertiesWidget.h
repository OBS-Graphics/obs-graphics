#pragma once

#include <QWidget>

class SceneDocument;
class QLineEdit;
class QLabel;

class ScenePropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScenePropertiesWidget(SceneDocument* doc, QWidget* parent = nullptr);

    void setSelection();

private slots:
    void onDocumentChanged();
    void onNameEditingFinished();

private:
    SceneDocument* m_doc;
    QLineEdit*     m_nameEdit;
    QLabel*        m_graphicsCountLabel;
    bool           m_updating{false};
};
