#pragma once

#include <QWidget>
#include <QElapsedTimer>
#include <QTimer>

struct scene;

class PreviewWidget : public QWidget {
    Q_OBJECT

    public:
        explicit PreviewWidget(QWidget* parent = nullptr);

        void play();
        void stop();
        void set_scene(const scene* sc);
        void seekMs(qint64 ms);

    Q_SIGNALS:
        void timeChanged(qint64 ms);

    protected:
        void paintEvent(QPaintEvent*) override;

    private:
        const scene* m_scene_ref = nullptr;
        bool m_playing;
        qint64 m_time_ms {};
        QElapsedTimer m_clock;
        QTimer m_tick;          //60 fps UI tick
};

