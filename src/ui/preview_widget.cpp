#include "ui/preview_widget.h"
#include "core/scene.h"
#include "core/scene_player.h"

#include <QPainter>
#include <QDir>

PreviewWidget::PreviewWidget(QWidget* parent):
    QWidget(parent),
    m_playing(false),
    m_time_ms(0){
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

    emit timeChanged(m_time_ms);
    update();
}

void PreviewWidget::set_scene(const scene* sc){
    m_scene_ref = sc;
    load_pixmaps();
    update();
}

void PreviewWidget::paintEvent(QPaintEvent*){
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 22));

    if(!m_scene_ref){
        return;
    }

    draw_scene(p, *m_scene_ref, m_time_ms, m_pixmap_cache);
}

void PreviewWidget::set_base_path(const QString& path){
    m_base_path = path;
}

void PreviewWidget::load_pixmaps(){
    m_pixmap_cache.clear();

    if(!m_scene_ref){
        return;
    }

    QDir base(m_base_path.isEmpty() ? "." : m_base_path);

    for(const auto& s : m_scene_ref->sprites){
        if(s.path.isEmpty() || m_pixmap_cache.contains(s.path)){
            continue;
        }
        
        const QString path = base.filePath(s.path);
        m_pixmap_cache.insert(s.path, QPixmap(path));
    }
}

