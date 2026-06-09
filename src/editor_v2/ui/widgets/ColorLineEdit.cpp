#include "ColorLineEdit.h"
#include <QRegularExpression>
#include <QTimer>

ColorLineEdit::ColorLineEdit(QWidget* parent) : QLineEdit(parent)
{
    connect(this, &QLineEdit::editingFinished, this, &ColorLineEdit::onEditingFinished);
    setColor(Qt::black);
}

QColor ColorLineEdit::color() const
{
    return m_color;
}

void ColorLineEdit::setColor(const QColor& c)
{
    m_color = c;
    m_updating = true;
    if (c.alpha() < 255)
        setText(QString("#%1%2%3%4")
                    .arg(c.alpha(), 2, 16, QChar('0'))
                    .arg(c.red(), 2, 16, QChar('0'))
                    .arg(c.green(), 2, 16, QChar('0'))
                    .arg(c.blue(), 2, 16, QChar('0'))
                    .toUpper());
    else
        setText(QString("#%1%2%3")
                    .arg(c.red(), 2, 16, QChar('0'))
                    .arg(c.green(), 2, 16, QChar('0'))
                    .arg(c.blue(), 2, 16, QChar('0'))
                    .toUpper());
    m_updating = false;
}

void ColorLineEdit::onEditingFinished()
{
    if (m_updating)
        return;
    QColor c = parse(text().trimmed());
    if (c.isValid()) {
        m_color = c;
        setColor(c);
        emit colorChanged(c);
    } else {
        setStyleSheet("background: #ffcccc;");
        QTimer::singleShot(600, this, [this] { setStyleSheet(""); });
    }
}

QColor ColorLineEdit::parse(const QString& text)
{
    QString t = text.trimmed();

    // rgb(r, g, b)
    static QRegularExpression rgbRe(R"(^rgb\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)$)");
    auto m = rgbRe.match(t);
    if (m.hasMatch()) {
        int r = m.captured(1).toInt();
        int g = m.captured(2).toInt();
        int b = m.captured(3).toInt();
        if (r > 255 || g > 255 || b > 255)
            return {};
        return QColor(r, g, b);
    }

    if (!t.startsWith('#'))
        return {};
    QString hex = t.mid(1);

    if (hex.length() == 3) {
        // #RGB → #RRGGBB
        hex = QString("%1%1%2%2%3%3").arg(hex[0]).arg(hex[1]).arg(hex[2]);
        return QColor('#' + hex);
    }

    if (hex.length() == 6) {
        QColor c('#' + hex);
        return c;
    }

    if (hex.length() == 8) {
        // #AARRGGBB — first 2 digits are alpha
        bool ok;
        int a = hex.left(2).toInt(&ok, 16);
        if (!ok)
            return {};
        QColor c('#' + hex.mid(2));
        if (!c.isValid())
            return {};
        c.setAlpha(a);
        return c;
    }

    return {};
}
