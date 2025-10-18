#pragma once

#include <QWidget>

class TimelineView : public QWidget {
    Q_OBJECT

public:
    explicit TimelineView(QWidget* parent = nullptr);
    QSize minimumSizeHint() const override { return { 400, 180 }; }

    void setBpm(double bpm);
    void setSubdivision(int div);
    void setPlayheadMs(qint64 ms);

signals:
    void requestSeek(qint64 ms);

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

private:
    //view state
    double m_pixels_per_ms; //zoom
    qint64 m_view_start_ms;
    qint64 m_playhead_ms;
    double m_bpm;
    int m_subdivision;

    qint64 beatMs() const { return static_cast<qint64>(60000.0 / m_bpm); }
    qint64 stepMs() const { return beatMs() / m_subdivision; }
    qint64 snapMs(qint64 t) const;
};
