#pragma once

#include <QtGlobal>

/**
 * The preview's current time, in storyboard milliseconds.
 *
 * Without a song (or outside it) the clock runs on wall time. While the song
 * plays, its reported position is the truth. Those reports are coarse (tens
 * of milliseconds apart), so the clock runs on wall time between them and is
 * pulled toward each report: gently for small drift, at once for big jumps
 * (the player stalled or skipped). While playing, the time doesn't go back
 * for a correction: when the song is behind, the picture holds until it
 * catches up. Only a song more than hold_limit_ms behind moves it back.
 *
 * All times are passed in, so the logic is testable without a clock or an
 * audio device: `now` is a steady wall-clock time in ms.
 */
class playback_clock{
    public:
        // Drift above this is corrected at once instead of gradually.
        static constexpr qint64 snap_threshold_ms = 100;

        // The share of the drift removed by each song report.
        static constexpr double correction_gain = 0.2;

        // A song further behind than this moves the time back instead of
        // holding the picture until it catches up.
        static constexpr qint64 hold_limit_ms = 500;

        void play(qint64 now);
        void pause(qint64 now);
        void seek(qint64 ms, qint64 now);

        // Playback speed (1 = normal). Keeps the current time.
        void set_rate(double rate, qint64 now);
        double rate() const { return m_rate; }

        bool is_playing() const { return m_playing; }

        // A position report from the song while it plays. Ignored when
        // paused: then the clock is the truth and the song follows it.
        void song_position(qint64 position_ms, qint64 now);

        // The time at `now`. While playing it only goes back after a seek
        // or when the song is more than hold_limit_ms behind.
        qint64 time(qint64 now);

    private:
        double raw_time(qint64 now) const;
        void anchor(double ms, qint64 now);

        bool m_playing = false;
        double m_rate = 1.0;
        double m_anchor_ms = 0;     // storyboard time at m_anchor_now
        qint64 m_anchor_now = 0;
        qint64 m_last_ms = 0;       // last time returned while playing
};
