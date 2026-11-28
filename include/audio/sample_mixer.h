#pragma once

#include <QAudioFormat>
#include <QIODevice>
#include <QObject>
#include <QVector>

#include <memory>
#include <vector>

class QAudioSink;

// A decoded sound: interleaved stereo floats at the mixer's sample rate.
using sound_ptr = std::shared_ptr<const QVector<float>>;

// One playing sound.
struct mixer_voice{
    sound_ptr sound;
    qsizetype position = 0;     // in floats (2 per frame)
    float gain = 1.0f;
};

// Adds every voice into `out` (`frames` stereo frames, overwritten), drops
// the voices that ended, and clamps the result to [-1, 1].
void mix_voices(std::vector<mixer_voice>& voices, float* out, qsizetype frames, float master_gain);

/**
 * Plays decoded sounds on top of each other through one audio output, so
 * starting a sound is instant and overlapping sounds don't cut each other.
 * Its own output, separate from the song's QMediaPlayer.
 */
class sample_mixer : public QObject{
    Q_OBJECT

    public:
        // At most this many sounds at once; the oldest stops for a new one.
        static constexpr int max_voices = 64;

        explicit sample_mixer(QObject* parent = nullptr);
        ~sample_mixer() override;

        // What sounds must be decoded to (stereo float at the output's rate).
        QAudioFormat sound_format() const;

        // Starts the audio output the first time; does nothing without one.
        void play(const sound_ptr& sound, float gain);

        // Stops every sound (pause, seek, new project).
        void stop_all();

        // Applies to sounds already playing too. 0 mutes.
        void set_master_gain(float gain);

        int voice_count() const;

    private:
        class device;

        bool ensure_started();

        QAudioFormat m_output_format;   // what the device gets
        std::unique_ptr<device> m_device;
        QAudioSink* m_sink = nullptr;
        bool m_failed = false;
        float m_master_gain = 1.0f;
};
