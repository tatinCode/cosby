#pragma once

#include "audio/sample_mixer.h"

#include <QAudioFormat>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

class QAudioBuffer;
class QAudioDecoder;

// Appends a decoded buffer to `out` as interleaved stereo floats (mono is
// copied to both sides; channels past two are dropped).
void append_as_stereo(const QAudioBuffer& buffer, QVector<float>& out);

// Linear resampling of interleaved stereo floats.
QVector<float> resample_stereo(const QVector<float>& in, int from_rate, int to_rate);

/**
 * Decoded storyboard sounds, by absolute path. Files are decoded one at a
 * time in the background; a file is decoded again when it changes on disk.
 */
class sample_bank : public QObject{
    Q_OBJECT

    public:
        explicit sample_bank(const QAudioFormat& format, QObject* parent = nullptr);
        ~sample_bank() override;

        // Decodes the files that aren't decoded (or changed since) and drops
        // the ones not listed any more.
        void load(const QStringList& paths);

        // Null until the file is decoded, or when it can't be.
        sound_ptr sound(const QString& path) const;

        bool is_loading() const { return m_decoder != nullptr; }

    Q_SIGNALS:
        // Once per file and change: the file exists but can't be played.
        void failed(const QString& path, const QString& reason);

        // Everything asked for is decoded (or failed).
        void idle();

    private:
        struct entry{
            QDateTime modified;
            sound_ptr sound;
            bool failed = false;
        };

        void start_next();
        void finish_current(bool ok, const QString& reason = {});

        QAudioFormat m_format;
        QHash<QString, entry> m_sounds;
        QSet<QString> m_wanted;
        QStringList m_queue;

        QAudioDecoder* m_decoder = nullptr;
        QString m_current;
        QDateTime m_current_modified;
        QVector<float> m_current_data;
        int m_current_rate = 0;
};
