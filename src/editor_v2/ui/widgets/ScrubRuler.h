#pragma once

#include <QWidget>

class ScrubRuler : public QWidget {
    Q_OBJECT
public:
    explicit ScrubRuler(QWidget* parent = nullptr);

    void setMaxDuration(float maxDuration);
    void setScrubTime(float t);

signals:
    void scrubChanged(float t);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

private:
    float timeAtX(int x) const;

    float m_maxDuration{1.0f};
    float m_scrubTime{0.0f};
};
