#include "ui/preview_widget.h"
#include "core/scene.h"
#include "core/scene_player.h"

#include <QPainter>
#include <QDir>

#include <algorithm>

PreviewWidget::PreviewWidget(QWidget* parent):
    QWidget(parent),
    m_playing(false),
    m_time_ms(0){
    setAutoFillBackground(false);
    connect(&m_tick, &QTimer::timeout, this, [this]{
            if(m_playing){
            m_time_ms = m_play_start_ms + m_clock.elapsed();
            emit timeChanged(m_time_ms);
            update();
            }
        });
    m_tick.start(1000/60);
}

void PreviewWidget::play(){
    if(m_playing){
        return;
    }

    // Resume from the current position instead of 0.
    m_play_start_ms = m_time_ms;
    m_clock.restart();
    m_playing = true;

    emit playingChanged(true);
}

void PreviewWidget::pause(){
    if(!m_playing){
        return;
    }

    m_time_ms = m_play_start_ms + m_clock.elapsed();
    m_playing = false;

    emit playingChanged(false);
}

void PreviewWidget::toggle_playback(){
    if(m_playing){
        pause();
    }
    else{
        play();
    }
}

void PreviewWidget::stop(){
    pause();
    seekMs(0);
}

bool PreviewWidget::is_playing() const{
    return m_playing;
}

void PreviewWidget::seekMs(qint64 ms){
    m_time_ms = std::max<qint64>(0, ms);

    // Keep playing from the new position.
    if(m_playing){
        m_play_start_ms = m_time_ms;
        m_clock.restart();
    }

    emit timeChanged(m_time_ms);
    update();
}

void PreviewWidget::set_scene(const scene* sc){
    m_has_scene = sc != nullptr;
    m_scene = sc ? *sc : scene{};
    load_pixmaps();
    update();
}

void PreviewWidget::paintEvent(QPaintEvent*){
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 22));

    if(!m_has_scene){
        p.setPen(QColor(110, 110, 120));
        p.drawText(rect(), Qt::AlignCenter, tr("Run the script (F5) to preview the storyboard"));
        return;
    }

    draw_scene(p, m_scene, m_time_ms, m_pixmap_cache, m_visible_layers);
}

void PreviewWidget::set_layer_visible(layer_t layer, bool visible){
    const unsigned bit = layer_bit(layer);
    m_visible_layers = visible ? (m_visible_layers | bit) : (m_visible_layers & ~bit);
    update();
}

void PreviewWidget::set_base_path(const QString& path){
    m_base_path = path;
}

void PreviewWidget::load_pixmaps(){
    m_pixmap_cache.clear();

    if(!m_has_scene){
        return;
    }

    QDir base(m_base_path.isEmpty() ? "." : m_base_path);

    for(const auto& s : m_scene.sprites){
        if(s.path.isEmpty() || m_pixmap_cache.contains(s.path)){
            continue;
        }
        
        const QString path = base.filePath(s.path);
        m_pixmap_cache.insert(s.path, QPixmap(path));
    }
}

