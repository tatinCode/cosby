#pragma once

#include "audio/playback_clock.h"
#include "audio/sample_schedule.h"
#include "core/scene.h"

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <functional>

class QAudioOutput;
class QMediaPlayer;
class sample_bank;
class sample_mixer;

/**
 * Play, pause and seek for the whole window: owns the preview time, plays
 * the song in step with it and fires storyboard samples as it passes them.
 * The preview and the timeline only follow timeChanged().
 *
 * Without a song (no project, missing file, past its end) the time still
 * runs, silently.
 */
class playback_controller : public QObject{
    Q_OBJECT

    public:
        // A steady clock in ms; tests pass their own.
        using now_function = std::function<qint64()>;

        explicit playback_controller(QObject* parent = nullptr, now_function now_fn = {});
        ~playback_controller() override;

        // An absolute path to the song; empty for none. Problems loading it
        // come through songProblem().
        void set_song(const QString& path);
        const QString& song() const { return m_song_path; }
        bool song_ready() const { return m_song_ready; }
        qint64 song_length() const { return m_song_length; }

        // The storyboard's samples, with files looked up in `folders`.
        void set_samples(const QVector<sample>& samples, const QStringList& folders);

        void play();
        void pause();
        void toggle_playback();
        void stop();                    // pause and go back to 0
        void seek(qint64 ms);

        // While the timeline's playhead is dragged: silent, the time only
        // follows seek(). Playback continues afterwards if it was on.
        void begin_scrub();
        void end_scrub();

        // What the user asked for; stays true during a scrub.
        bool is_playing() const { return m_playing; }
        qint64 time() const { return m_time; }

        // Linear gains, 0-1.
        void set_music_volume(float volume);
        void set_sample_volume(float volume);
        void set_muted(bool muted);

        // 0.25-1 for checking timing; pitch is kept where Qt can.
        void set_rate(double rate);
        double rate() const { return m_clock.rate(); }

        // One frame of playback. Runs on a 60 fps timer while playing;
        // public so tests can step it.
        void tick();

    Q_SIGNALS:
        void timeChanged(qint64 ms);
        void playingChanged(bool playing);

        // The song file exists but can't be played (or stopped loading).
        void songProblem(const QString& message);

        // A sample file exists but can't be decoded.
        void sampleProblem(const QString& path, const QString& reason);

        // A sample's time was passed (index into the samples given to
        // set_samples), whether or not its sound could be played.
        void sampleTriggered(qsizetype index);

    private:
        qint64 now() const;
        bool running() const { return m_playing && !m_scrubbing; }
        void start_running();
        void stop_running();
        void sync_song();
        void set_time(qint64 ms);
        void apply_gains();

        now_function m_now;
        QElapsedTimer m_elapsed;
        QTimer m_tick;

        playback_clock m_clock;
        bool m_playing = false;
        bool m_scrubbing = false;
        qint64 m_time = 0;

        QMediaPlayer* m_player = nullptr;
        QAudioOutput* m_music_output = nullptr;
        QString m_song_path;
        bool m_song_ready = false;
        bool m_song_finished = false;   // played to its end; cleared by a seek
        qint64 m_song_length = 0;

        QVector<sample> m_samples;
        QStringList m_sample_files;     // absolute path per sample; empty when missing
        sample_schedule m_schedule;
        qint64 m_samples_done_until = 0;    // samples up to here were fired
        sample_mixer* m_mixer = nullptr;
        sample_bank* m_bank = nullptr;

        float m_music_volume = 1.0f;
        float m_sample_volume = 1.0f;
        bool m_muted = false;
};
