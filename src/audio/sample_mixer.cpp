#include "audio/sample_mixer.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QMediaDevices>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

void mix_voices(std::vector<mixer_voice>& voices, float* out, qsizetype frames, float master_gain){
    const qsizetype count = frames * 2;
    std::fill(out, out + count, 0.0f);

    for(mixer_voice& voice : voices){
        const QVector<float>& data = *voice.sound;
        const qsizetype n = std::min(count, data.size() - voice.position);
        const float gain = voice.gain * master_gain;
        const float* src = data.constData() + voice.position;

        for(qsizetype i = 0; i < n; ++i){
            out[i] += src[i] * gain;
        }

        voice.position += n;
    }

    std::erase_if(voices, [](const mixer_voice& v){ return v.position >= v.sound->size(); });

    for(qsizetype i = 0; i < count; ++i){
        out[i] = std::clamp(out[i], -1.0f, 1.0f);
    }
}

/**
 * What the audio output pulls from: the mix of the playing voices, converted
 * to the output's format. Never runs dry, so the output never stops; it gets
 * silence when nothing plays. read may run on the audio thread.
 */
class sample_mixer::device : public QIODevice{
    public:
        explicit device(const QAudioFormat& format): m_format(format){
            open(QIODevice::ReadOnly);
        }

        bool isSequential() const override { return true; }

        qint64 bytesAvailable() const override {
            // Always enough for one more period.
            return QIODevice::bytesAvailable() + (qint64(1) << 20);
        }

        void add(mixer_voice voice){
            std::lock_guard lock(m_mutex);

            if(int(m_voices.size()) >= max_voices){
                m_voices.erase(m_voices.begin());
            }

            m_voices.push_back(std::move(voice));
        }

        void clear(){
            std::lock_guard lock(m_mutex);
            m_voices.clear();
        }

        int count() const{
            std::lock_guard lock(m_mutex);
            return int(m_voices.size());
        }

        std::atomic<float> master_gain{1.0f};

    protected:
        qint64 readData(char* data, qint64 max_size) override {
            const int bytes_per_frame = m_format.bytesPerFrame();

            if(bytes_per_frame <= 0){
                return 0;
            }

            const qsizetype frames = qsizetype(max_size / bytes_per_frame);
            m_mix.resize(size_t(frames) * 2);

            {
                std::lock_guard lock(m_mutex);
                mix_voices(m_voices, m_mix.data(), frames, master_gain.load());
            }

            write_frames(data, frames);
            return qint64(frames) * bytes_per_frame;
        }

        qint64 writeData(const char*, qint64) override { return -1; }

    private:
        // m_mix (stereo float) into the output's channels and sample type.
        void write_frames(char* data, qsizetype frames) const{
            const int channels = m_format.channelCount();
            const int bytes = m_format.bytesPerSample();

            for(qsizetype f = 0; f < frames; ++f){
                const float left = m_mix[size_t(f) * 2];
                const float right = m_mix[size_t(f) * 2 + 1];

                for(int c = 0; c < channels; ++c){
                    const float v = channels == 1 ? (left + right) * 0.5f
                                  : c == 0 ? left
                                  : c == 1 ? right
                                  : 0.0f;

                    write_sample(data + (f * channels + c) * bytes, v);
                }
            }
        }

        void write_sample(char* out, float v) const{
            switch(m_format.sampleFormat()){
                case QAudioFormat::Float:{
                    std::memcpy(out, &v, sizeof v);
                    break;
                }
                case QAudioFormat::Int16:{
                    const qint16 s = qint16(v * 32767.0f);
                    std::memcpy(out, &s, sizeof s);
                    break;
                }
                case QAudioFormat::Int32:{
                    const qint32 s = qint32(double(v) * 2147483647.0);
                    std::memcpy(out, &s, sizeof s);
                    break;
                }
                case QAudioFormat::UInt8:{
                    *reinterpret_cast<quint8*>(out) = quint8((v + 1.0f) * 127.5f);
                    break;
                }
                default:
                    break;
            }
        }

        QAudioFormat m_format;
        mutable std::mutex m_mutex;
        std::vector<mixer_voice> m_voices;
        std::vector<float> m_mix;
};

namespace{
    QAudioFormat choose_output_format(const QAudioDevice& output){
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Float);

        if(output.isNull()){
            return format;
        }

        const int rate = output.preferredFormat().sampleRate();
        if(rate > 0){
            format.setSampleRate(rate);
        }

        if(output.isFormatSupported(format)){
            return format;
        }

        format.setSampleFormat(QAudioFormat::Int16);
        if(output.isFormatSupported(format)){
            return format;
        }

        return output.preferredFormat();
    }
}

sample_mixer::sample_mixer(QObject* parent):
    QObject(parent),
    m_output_format(choose_output_format(QMediaDevices::defaultAudioOutput())){
}

sample_mixer::~sample_mixer(){
    if(m_sink){
        m_sink->stop();
        delete m_sink;
    }
}

QAudioFormat sample_mixer::sound_format() const{
    QAudioFormat format;
    format.setSampleRate(m_output_format.sampleRate() > 0 ? m_output_format.sampleRate() : 48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    return format;
}

bool sample_mixer::ensure_started(){
    if(m_sink){
        return true;
    }

    if(m_failed){
        return false;
    }

    const QAudioDevice output = QMediaDevices::defaultAudioOutput();

    if(output.isNull() || !m_output_format.isValid()){
        m_failed = true;
        return false;
    }

    m_device = std::make_unique<device>(m_output_format);
    m_device->master_gain = m_master_gain;
    m_sink = new QAudioSink(output, m_output_format);

    // About 40 ms: short enough that hits land on time.
    m_sink->setBufferSize(m_output_format.bytesForDuration(40000));
    m_sink->start(m_device.get());

    if(m_sink->error() != QAudio::NoError){
        delete m_sink;
        m_sink = nullptr;
        m_device.reset();
        m_failed = true;
        return false;
    }

    return true;
}

void sample_mixer::play(const sound_ptr& sound, float gain){
    if(!sound || sound->isEmpty() || !ensure_started()){
        return;
    }

    m_device->add(mixer_voice{sound, 0, gain});
}

void sample_mixer::stop_all(){
    if(m_device){
        m_device->clear();
    }
}

void sample_mixer::set_master_gain(float gain){
    m_master_gain = gain;

    if(m_device){
        m_device->master_gain = gain;
    }
}

int sample_mixer::voice_count() const{
    return m_device ? m_device->count() : 0;
}
