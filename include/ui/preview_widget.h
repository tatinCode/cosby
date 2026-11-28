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
        QStringList m_asset_folders;
        QHash<QString, QPixmap> m_pixmap_cache;
};

