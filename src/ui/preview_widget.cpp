#include "ui/preview_widget.h"
#include "core/scene.h"
#include "core/scene_player.h"
#include "project/asset_paths.h"

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

namespace {
    // osu!'s storyboard space: 640x480, shown widescreen as 854x480 with
    // the 4:3 area in the middle (x from -107 to 747).
    constexpr double playfield_width = 640.0;
    constexpr double widescreen_width = 854.0;
    constexpr double playfield_height = 480.0;
    constexpr double widescreen_left = (playfield_width - widescreen_width) / 2.0;
}

QRectF PreviewWidget::playfield_rect() const{
    const double scale = std::min(width() / widescreen_width, height() / playfield_height);
    const QSizeF size(widescreen_width * scale, playfield_height * scale);

    return QRectF(QPointF((width() - size.width()) / 2.0, (height() - size.height()) / 2.0), size);
}

void PreviewWidget::paintEvent(QPaintEvent*){
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 22));

    if(!m_has_scene){
        p.setPen(QColor(110, 110, 120));
        p.drawText(rect(), Qt::AlignCenter, tr("Run the script (F5) to preview the storyboard"));
        return;
    }

    const QRectF screen = playfield_rect();
    const double scale = screen.height() / playfield_height;

    p.fillRect(screen, Qt::black);
    p.setClipRect(screen);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    p.save();
    p.translate(screen.left() - widescreen_left * scale, screen.top());
    p.scale(scale, scale);
    draw_scene(p, m_scene, m_time_ms, m_pixmap_cache, m_visible_layers);
    p.restore();

    // Edges of the 4:3 area, which every player sees.
    p.setPen(QPen(QColor(255, 255, 255, 40), 0, Qt::DashLine));
    const double left = screen.left() - widescreen_left * scale;
    const double right = left + playfield_width * scale;
    p.drawLine(QPointF(left, screen.top()), QPointF(left, screen.bottom()));
    p.drawLine(QPointF(right, screen.top()), QPointF(right, screen.bottom()));
}

void PreviewWidget::set_layer_visible(layer_t layer, bool visible){
    const unsigned bit = layer_bit(layer);
    m_visible_layers = visible ? (m_visible_layers | bit) : (m_visible_layers & ~bit);
    update();
}

void PreviewWidget::set_asset_folders(const QStringList& folders){
    m_asset_folders = folders;
    load_pixmaps();
    update();
}

void PreviewWidget::load_pixmaps(){
    m_pixmap_cache.clear();

    if(!m_has_scene){
        return;
    }

    for(const auto& s : m_scene.sprites){
        if(s.path.isEmpty() || m_pixmap_cache.contains(s.path)){
            continue;
        }

        // Missing images stay null and are drawn as a placeholder box.
        m_pixmap_cache.insert(s.path, QPixmap(find_asset(m_asset_folders, s.path)));
    }
}

