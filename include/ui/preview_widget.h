#pragma once

#include <QWidget>
#include <QElapsedTimer>
#include <QTimer>
#include <QHash>
#include <QPixmap>

#include "core/scene.h"

class PreviewWidget : public QWidget {
    Q_OBJECT

    public:
        explicit PreviewWidget(QWidget* parent = nullptr);

        void play();                // resumes from the current time
        void pause();
        void toggle_playback();
        void stop();                // pause and go back to 0
        bool is_playing() const;
        // Copies the scene (cheap, implicitly shared); nullptr clears it.
        void set_scene(const scene* sc);
        // Preview only: hidden layers are still exported.
        void set_layer_visible(layer_t layer, bool visible);
        void seekMs(qint64 ms);
        void set_base_path(const QString& path);

    Q_SIGNALS:
        void timeChanged(qint64 ms);
        void playingChanged(bool playing);

    protected:
        void paintEvent(QPaintEvent*) override;

    private:
        scene m_scene;
        bool m_has_scene = false;
        unsigned m_visible_layers = 0x1f;  // bit per layer_t
        bool m_playing;
        qint64 m_time_ms {};
        qint64 m_play_start_ms {};  // m_time_ms when playback (re)started
        QElapsedTimer m_clock;
        QTimer m_tick;          //60 fps UI tick

        void load_pixmaps();
        QString m_base_path;
        QHash<QString, QPixmap> m_pixmap_cache;
};

