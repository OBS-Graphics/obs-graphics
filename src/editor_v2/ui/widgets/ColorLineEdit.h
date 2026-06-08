#pragma once
#include <QLineEdit>
#include <QColor>

class ColorLineEdit : public QLineEdit {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    explicit ColorLineEdit(QWidget* parent = nullptr);
    QColor color() const;
    void setColor(const QColor& c);
signals:
    void colorChanged(const QColor&);
private slots:
    void onEditingFinished();
private:
    static QColor parse(const QString& text);
    QColor m_color;
    bool m_updating{false};
};
