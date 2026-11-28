#include "ui/preview_widget.h"
#include "core/scene.h"
#include "core/scene_player.h"
#include "core/sprite_timing.h"
#include "project/asset_paths.h"

#include <QPainter>
#include <QDir>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>

PreviewWidget::PreviewWidget(QWidget* parent):
    QWidget(parent){
    setAutoFillBackground(false);
}

void PreviewWidget::set_time_ms(qint64 ms){
    if(ms == m_time_ms){
        return;
    }

    m_time_ms = ms;
    update();
}

void PreviewWidget::set_placeholder_background(const QString& path){
    // Always reloaded: a beatmap update can replace the file under the same name.
    m_placeholder = path.isEmpty() ? QPixmap() : QPixmap(path);
    m_placeholder_scaled = QPixmap();
    update();
}

void PreviewWidget::show_note(const QString& text, int duration_ms){
    if(!m_note_fade){
        m_note_fade = new QVariantAnimation(this);
        m_note_fade->setStartValue(1.0);
        m_note_fade->setKeyValueAt(0.8, 1.0);   // there for most of it, then a quick fade
        m_note_fade->setEndValue(0.0);

        connect(m_note_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& value){
            m_note_opacity = value.toDouble();
            update();
        });
    }

    m_note_fade->stop();
    m_note = text;
    m_note_opacity = text.isEmpty() ? 0.0 : 1.0;

    if(!text.isEmpty()){
        m_note_fade->setDuration(std::max(1, duration_ms));
        m_note_fade->start();
    }

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

    const QRectF screen = playfield_rect();

    p.fillRect(screen, Qt::black);
    p.setClipRect(screen);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    if(m_scene.sprites.isEmpty()){
        draw_placeholder(p, screen);
    }

    if(m_has_scene){
        draw_storyboard(p, screen);
    }

    draw_note(p);
}

void PreviewWidget::draw_storyboard(QPainter& p, const QRectF& screen){
    const double scale = screen.height() / playfield_height;

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

    if(m_asset_folders.isEmpty() && !m_scene.sprites.isEmpty()){
        p.setClipping(false);
        p.setPen(QColor(255, 0, 255));
        p.drawText(screen.adjusted(8, 0, -8, -6), Qt::AlignBottom | Qt::AlignLeft,
                tr("No project open: images can't be loaded"));
    }
}

void PreviewWidget::draw_note(QPainter& p){
    if(m_note_opacity <= 0.0 || m_note.isEmpty()){
        return;
    }

    p.save();
    p.setClipping(false);
    p.setRenderHint(QPainter::Antialiasing);

    constexpr int margin = 10;
    constexpr int padding = 8;
    const QRect text = p.fontMetrics().boundingRect(m_note);
    const QRectF box(width() - margin - text.width() - 2 * padding, margin,
            text.width() + 2 * padding, text.height() + padding);

    p.setOpacity(m_note_opacity);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 150));
    p.drawRoundedRect(box, 4, 4);
    p.setPen(QColor(255, 255, 255, 190));
    p.drawText(box, Qt::AlignCenter, m_note);
    p.restore();
}

void PreviewWidget::draw_placeholder(QPainter& p, const QRectF& screen){
    if(m_placeholder.isNull()){
        return;
    }

    // Fills the screen and is centred, cropping what sticks out, like osu!.
    const QSize target = m_placeholder.size().scaled(screen.size().toSize(), Qt::KeepAspectRatioByExpanding);

    // Scaled once per size, not on every frame.
    if(m_placeholder_scaled.size() != target){
        m_placeholder_scaled = m_placeholder.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    const QPointF top_left(screen.center().x() - target.width() / 2.0,
                           screen.center().y() - target.height() / 2.0);
    p.drawPixmap(top_left, m_placeholder_scaled);
    p.fillRect(screen, QColor(0, 0, 0, int(std::lround(placeholder_dim * 255))));
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
        // Keyed by the path as the script wrote it (each frame's, for
        // animations); draw_sprite() looks them up the same way.
        for(const QString& path : image_paths(s)){
            if(path.isEmpty() || m_pixmap_cache.contains(path)){
                continue;
            }

            // Missing images stay null and are drawn as a placeholder.
            m_pixmap_cache.insert(path, QPixmap(find_asset(m_asset_folders, path)));
        }
    }
}

