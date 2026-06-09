#ifndef ADAPTIVESTACK_H
#define ADAPTIVESTACK_H

#include <QList>
#include <QWidget>

class AdaptiveStack : public QWidget {
    Q_OBJECT
public:
    explicit AdaptiveStack(QWidget* parent = nullptr);
    void addWidget(QWidget* w);
    void setCurrentIndex(int index);
    int currentIndex() const;
    QWidget* currentWidget() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void currentChanged(int index);

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    QList<QWidget*> m_pages;
    int m_current = -1;
};

#endif // ADAPTIVESTACK_H
