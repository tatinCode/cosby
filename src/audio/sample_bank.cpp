#include "audio/sample_bank.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QFileInfo>
#include <QUrl>

#include <cmath>

void append_as_stereo(const QAudioBuffer& buffer, QVector<float>& out){
    const QAudioFormat format = buffer.format();
    const int channels = format.channelCount();
    const int bytes = format.bytesPerSample();

    if(!buffer.isValid() || channels <= 0 || bytes <= 0){
        return;
    }

    const auto* data = buffer.constData<char>();
    const qsizetype frames = buffer.frameCount();

    out.reserve(out.size() + frames * 2);

    for(qsizetype f = 0; f < frames; ++f){
        const char* frame = data + f * channels * bytes;
        const float left = format.normalizedSampleValue(frame);
        const float right = channels > 1 ? format.normalizedSampleValue(frame + bytes) : left;

        out.append(left);
        out.append(right);
    }
}

QVector<float> resample_stereo(const QVector<float>& in, int from_rate, int to_rate){
    if(from_rate <= 0 || to_rate <= 0 || from_rate == to_rate || in.size() < 4){
        return in;
    }

    const qsizetype in_frames = in.size() / 2;
    const auto out_frames = qsizetype(std::llround(double(in_frames) * to_rate / from_rate));
    const double step = double(from_rate) / to_rate;

    QVector<float> out(out_frames * 2);

    for(qsizetype f = 0; f < out_frames; ++f){
        const double pos = f * step;
        const auto i = std::min(qsizetype(pos), in_frames - 1);
        const auto j = std::min(i + 1, in_frames - 1);
        const auto frac = float(pos - double(i));

        for(int c = 0; c < 2; ++c){
            out[f * 2 + c] = in[i * 2 + c] + (in[j * 2 + c] - in[i * 2 + c]) * frac;
        }
    }

    return out;
}

sample_bank::sample_bank(const QAudioFormat& format, QObject* parent):
    QObject(parent),
    m_format(format){
}

sample_bank::~sample_bank(){
    if(m_decoder){
        m_decoder->disconnect(this);
        m_decoder->stop();
    }
}

void sample_bank::load(const QStringList& paths){
    m_wanted = QSet<QString>(paths.begin(), paths.end());
    const QSet<QString>& wanted = m_wanted;

    // Drop sounds the scene doesn't use any more.
    for(auto it = m_sounds.begin(); it != m_sounds.end();){
        it = wanted.contains(it.key()) ? std::next(it) : m_sounds.erase(it);
    }

    m_queue.clear();

    for(const QString& path : wanted){
        const QFileInfo info(path);

        if(!info.isFile()){
            m_sounds.remove(path);      // missing: the asset check reports it
            continue;
        }

        const auto known = m_sounds.constFind(path);
        const bool up_to_date = known != m_sounds.constEnd() && known->modified == info.lastModified();
        const bool decoding = path == m_current && m_current_modified == info.lastModified();

        if(!up_to_date && !decoding){
            m_queue.append(path);
        }
    }

    m_queue.sort();     // a stable order, so failures are reported the same way each run

    if(!m_decoder){
        start_next();
    }
}

sound_ptr sample_bank::sound(const QString& path) const{
    const auto it = m_sounds.constFind(path);
    return it == m_sounds.constEnd() ? nullptr : it->sound;
}

void sample_bank::start_next(){
    if(m_queue.isEmpty()){
        emit idle();
        return;
    }

    m_current = m_queue.takeFirst();
    m_current_modified = QFileInfo(m_current).lastModified();
    m_current_data.clear();
    m_current_rate = 0;

    m_decoder = new QAudioDecoder(this);

    if(!m_decoder->isSupported()){
        finish_current(false, tr("no audio decoder is available"));
        return;
    }

    // Ask for the mixer's format; the backend may still hand out another one,
    // which append_as_stereo() and resample_stereo() deal with.
    m_decoder->setAudioFormat(m_format);
    m_decoder->setSource(QUrl::fromLocalFile(m_current));

    connect(m_decoder, &QAudioDecoder::bufferReady, this, [this]{
        while(m_decoder && m_decoder->bufferAvailable()){
            const QAudioBuffer buffer = m_decoder->read();
            m_current_rate = buffer.format().sampleRate();
            append_as_stereo(buffer, m_current_data);
        }
    });
    connect(m_decoder, &QAudioDecoder::finished, this, [this]{
        finish_current(true);
    });
    connect(m_decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this, [this]{
        finish_current(false, m_decoder->errorString());
    });

    m_decoder->start();
}

void sample_bank::finish_current(bool ok, const QString& reason){
    QAudioDecoder* decoder = m_decoder;
    m_decoder = nullptr;

    // Not deleted here: this may run inside one of its signals.
    decoder->disconnect(this);
    decoder->deleteLater();

    // Kept only if the scene still uses it (load() may have run meanwhile).
    if(m_wanted.contains(m_current)){
        entry e;
        e.modified = m_current_modified;

        if(ok && !m_current_data.isEmpty()){
            e.sound = std::make_shared<const QVector<float>>(
                    resample_stereo(m_current_data, m_current_rate, m_format.sampleRate()));
        }
        else{
            e.failed = true;
            emit failed(m_current, reason.isEmpty() ? tr("it has no audio") : reason);
        }

        m_sounds.insert(m_current, e);
    }

    m_current.clear();
    m_current_data = {};

    start_next();
}
