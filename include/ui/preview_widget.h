#pragma once

#include <QWidget>
#include <QHash>
#include <QPixmap>

#include "core/scene.h"

// Draws the storyboard at the time it's given (playback_controller owns
// the time).
class PreviewWidget : public QWidget {
    Q_OBJECT

    public:
        explicit PreviewWidget(QWidget* parent = nullptr);

        void set_time_ms(qint64 ms);
        qint64 time_ms() const { return m_time_ms; }

        // Copies the scene (cheap, implicitly shared); nullptr clears it.
        void set_scene(const scene* sc);
        // Preview only: hidden layers are still exported.
        void set_layer_visible(layer_t layer, bool visible);

        // Folders searched for sprite images, in order (the project's
        // assets/ first, then the beatmap copy).
        void set_asset_folders(const QStringList& folders);

        // The beatmap's background image (absolute path; empty for none).
        // Shown dimmed, as a stand-in, while the storyboard has no sprites:
        // before the script runs or when it makes none. Never exported.
        // A missing or unreadable image leaves the preview black.
        void set_placeholder_background(const QString& path);
        bool has_placeholder_background() const { return !m_placeholder.isNull(); }

        // How much the placeholder is darkened (0 = as is, 1 = black).
        static constexpr double placeholder_dim = 0.4;

        // Where the widescreen playfield (854x480, x from -107) lands in the
        // widget; storyboard coordinates are mapped onto it.
        QRectF playfield_rect() const;

    protected:
        void paintEvent(QPaintEvent*) override;

    private:
        scene m_scene;
        bool m_has_scene = false;
        unsigned m_visible_layers = 0x1f;  // bit per layer_t
        qint64 m_time_ms {};

        void load_pixmaps();
        void draw_placeholder(QPainter& p, const QRectF& screen);

        QPixmap m_placeholder;
        QPixmap m_placeholder_scaled;   // m_placeholder at its last drawn size

        QStringList m_asset_folders;
        QHash<QString, QPixmap> m_pixmap_cache;
};

