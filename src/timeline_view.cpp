#include "timeline_view.h"

#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>


TimelineView::TimelineView(QWidget* parent):
    QWidget(parent),
    m_pixels_per_ms(0.2),
    m_view_start_ms(0),
    m_playhead_ms(0),
    m_bpm(180.0),
    m_subdivision(4)    //1/4 by default
{
    setMouseTracking(true);
}

void TimelineView::setBpm(double bpm){
    m_bpm = bpm;
    update();
}

void TimelineView::setSubdivision(int div){
    m_subdivision = std::max(1, div);
    update();
}

void TimelineView::setPlayheadMs(qint64 ms){
    m_playhead_ms = ms;
    update();
}

void TimelineView::paintEvent(QPaintEvent*){
    QPainter p(this);
    p.fillRect(rect(), QColor(28, 28, 28));
    const QRect r = rect();

    //visible time range
    const qint64 t0 = m_view_start_ms;
    const qint64 t1 = m_view_start_ms + qint64(r.width() / m_pixels_per_ms);

    //grid: choose tick every stepMs (minor) and beatMs (major)
    const qint64 minor = stepMs();
    const qint64 major = beatMs();

    //finds the first minor tick >= t0
    qint64 first_tick = (t0 / minor) * minor;
    if(first_tick < t0){
        first_tick += minor;
    }

    //draws the ticks
    QPen minor_pen(QColor(70, 70, 70));
    QPen major_pen(QColor(110, 110, 110));
    QFont f = p.font();
    f.setPointSizeF(10);
    p.setFont(f);

    for(qint64 t = first_tick; t <= t1; t += minor){
        const int x = int((t - m_view_start_ms) * m_pixels_per_ms);
        const bool is_major = (t % major) == 0;

        p.setPen(is_major ? major_pen : minor_pen);
        const int h = is_major ? 22 : 12;
        p.drawLine(x, r.bottom() - h, x, r.bottom());

        if(is_major){
            //label measure: beat
            const qint64 beat_index = t / major;
            const QString label = QString::number(beat_index);
            p.drawText(x + 3, 14, label);
        }
    }

    //playhead
    p.setPen(QPen(QColor(200, 160, 60), 2));
    const int px = int((m_playhead_ms - m_view_start_ms) * m_pixels_per_ms);
    p.drawLine(px, r.top(), px, r.bottom());

    //ruler background
    p.fillRect(QRect(r.left(), r.top(), r.width(), 18), QColor(36, 36, 36));
}

void TimelineView::wheelEvent(QWheelEvent* e){
    //ctrl + wheel = zoom at cursor's place
    //otherwise it would pan horizontally
    if(e->modifiers() & Qt::ControlModifier){
        const double delta = e->angleDelta().y() / 120.0;
        const double factor = std::pow(1.15, delta);
        const int x = e->position().x();
        const qint64 t_at_cursor = m_view_start_ms + qint64(x / m_pixels_per_ms);

        m_pixels_per_ms = std::clamp(m_pixels_per_ms * factor, 0.02, 5.0);

        //keep cursor time stable
        m_view_start_ms = t_at_cursor - qint64(x / m_pixels_per_ms);
        if(m_view_start_ms < 0){
            m_view_start_ms = 0;
        }
    }
    else{
        //pan by wheel
        const int dy = e->angleDelta().y();
        const qint64 dt = qint64(-dy / m_pixels_per_ms * 0.25);
        m_view_start_ms = std::max<qint64>(0, m_view_start_ms + dt);
    }

    update();
}

void TimelineView::mousePressEvent(QMouseEvent* e){
    if(e->button() == Qt::LeftButton){
        const qint64 t = m_view_start_ms + qint64(e->position().x() / m_pixels_per_ms);
        const qint64 snapped = (e->modifiers() & Qt::AltModifier) ? t : snapMs(t);
        emit requestSeek(snapped);
    }
}

void TimelineView::mouseMoveEvent(QMouseEvent* e){
    if(e->buttons() & Qt::RightButton){
        //right-drag to pan
        static QPointF last = e -> position();
        const QPointF curr = e->position();
        const qreal dx = curr.x() - last.x();

        m_view_start_ms = std::max<qint64>(0, m_view_start_ms - qint64(dx / m_pixels_per_ms));
        last = curr;
        update();
    }
}

qint64 TimelineView::snapMs(qint64 ms) const{
    return ms;
}

