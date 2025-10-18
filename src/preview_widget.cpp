#include "preview_widget.h"

#include <QPainter>

PreviewWidget::PreviewWidget(QWidget* parent):
    QWidget(parent),
    m_playing(false),
    m_time_ms(0)
{
    setAutoFillBackground(false);
    connect(&m_tick, &QTimer::timeout, this, [this]{
            if(m_playing){
            m_time_ms = m_clock.elapsed();
            emit timeChanged(m_time_ms);
            update();
            }
        });
    m_tick.start(1000/60);
}

void PreviewWidget::play(){
    m_playing = true;
    m_clock.restart();
}

void PreviewWidget::stop(){
    m_playing = false;
}

void PreviewWidget::seekMs(qint64 ms){
    m_time_ms = ms;

    if(m_playing){
        //m_clock.restart(), m_clock = QElapsedTimer(), m_clock().start();
        m_clock.restart();
    }

    emit timeChanged(m_time_ms);

    update();
}

void PreviewWidget::paintEvent(QPaintEvent*){
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 22));

    //placeholder preview content
    p.setPen(Qt::white);
    p.drawText(20, 30, QString("Preview t=%1 ms").arg(m_time_ms));
    p.drawRect(rect().adjusted(10, 10, -10, -10));
}

