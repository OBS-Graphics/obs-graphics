#pragma once

#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#include <string>

class SceneDocument;

class GraphicProperties : public QWidget {
    Q_OBJECT
public:
    explicit GraphicProperties(SceneDocument* doc, QWidget* parent = nullptr);

    void setSelection(const std::string& graphicId);

private slots:
    void onDocumentChanged();
    void onIdEditingFinished();
    void onZOrderChanged(int value);

private:
    SceneDocument* m_doc;
    std::string m_graphicId;
    bool m_updating{false};

    QLineEdit* m_idEdit;
    QSpinBox* m_zOrderSpin;
};
