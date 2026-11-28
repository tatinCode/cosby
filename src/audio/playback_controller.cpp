#include "audio/playback_controller.h"

#include "audio/sample_bank.h"
#include "audio/sample_mixer.h"
#include "project/asset_paths.h"

#include <QAudioOutput>
#include <QDir>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QUrl>

#include <algorithm>

playback_controller::playback_controller(QObject* parent, now_function now_fn):
    QObject(parent),
    m_now(std::move(now_fn)),
    m_player(new QMediaPlayer(this)),
    m_music_output(new QAudioOutput(this)),
    m_mixer(new sample_mixer(this)),
    m_bank(nullptr){
    m_elapsed.start();
    m_bank = new sample_bank(m_mixer->sound_format(), this);

    m_tick.setTimerType(Qt::PreciseTimer);
    m_tick.setInterval(1000 / 60);
    connect(&m_tick, &QTimer::timeout, this, &playback_controller::tick);

    m_player->setAudioOutput(m_music_output);

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    // Slowed down songs keep their pitch, so they're still recognisable.
    if(m_player->pitchCompensationAvailability() == QMediaPlayer::PitchCompensationAvailability::Available){
        m_player->setPitchCompensation(true);
    }
#endif

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status){
        switch(status){
            case QMediaPlayer::LoadedMedia:
                m_song_ready = true;
                m_song_finished = false;
                m_song_length = m_player->duration();
                sync_song();
                break;
            case QMediaPlayer::EndOfMedia:
                // The storyboard can go on after the song; the time keeps running.
                m_song_finished = true;
                break;
            case QMediaPlayer::InvalidMedia:
                m_song_ready = false;
                break;
            default:
                break;
        }
    });
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 duration){
        m_song_length = duration;
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString& message){
        if(error == QMediaPlayer::NoError){
            return;
        }

        m_song_ready = false;
        emit songProblem(tr("Can't play the song %1: %2").arg(QFileInfo(m_song_path).fileName(), message));
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 position){
        if(running() && m_song_ready && m_player->playbackState() == QMediaPlayer::PlayingState){
            m_clock.song_position(position, now());
        }
    });

    connect(m_bank, &sample_bank::failed, this, &playback_controller::sampleProblem);

    apply_gains();
}

playback_controller::~playback_controller(){
    m_tick.stop();
    m_player->stop();
}

qint64 playback_controller::now() const{
    return m_now ? m_now() : m_elapsed.elapsed();
}

void playback_controller::set_song(const QString& path){
    // Always reloaded: a beatmap update replaces the file under the same name.
    m_player->stop();
    m_player->setSource(QUrl());
    m_song_ready = false;
    m_song_finished = false;
    m_song_length = 0;
    m_song_path = path;

    if(path.isEmpty()){
        return;
    }

    if(!QFileInfo(path).isFile()){
        emit songProblem(tr("Song %1 not found: the preview plays without sound")
                         .arg(QDir::toNativeSeparators(path)));
        return;
    }

    m_player->setSource(QUrl::fromLocalFile(path));
}

void playback_controller::set_samples(const QVector<sample>& samples, const QStringList& folders){
    m_mixer->stop_all();

    m_samples = samples;
    m_sample_files.clear();
    m_schedule = sample_schedule(samples);

    QStringList files;

    for(const sample& s : samples){
        const QString file = find_asset(folders, s.path);
        m_sample_files.append(file);

        if(!file.isEmpty() && sample_schedule::plays_in_preview(s.layer) && !files.contains(file)){
            files.append(file);
        }
    }

    m_bank->load(files);

    // Samples at the current time don't go off just because the script ran.
    m_samples_done_until = m_time;
}

void playback_controller::play(){
    if(m_playing){
        return;
    }

    m_playing = true;

    if(!m_scrubbing){
        start_running();
    }

    emit playingChanged(true);
}

void playback_controller::pause(){
    if(!m_playing){
        return;
    }

    m_playing = false;

    if(!m_scrubbing){
        stop_running();
    }

    emit playingChanged(false);
}

void playback_controller::toggle_playback(){
    if(m_playing){
        pause();
    }
    else{
        play();
    }
}

void playback_controller::stop(){
    pause();
    seek(0);
}

void playback_controller::seek(qint64 ms){
    ms = std::max<qint64>(0, ms);

    m_clock.seek(ms, now());
    m_mixer->stop_all();
    m_samples_done_until = ms;
    m_song_finished = false;

    if(m_song_ready && ms < m_song_length){
        m_player->setPosition(ms);
    }

    m_time = ms;
    sync_song();

    emit timeChanged(ms);
}

void playback_controller::begin_scrub(){
    if(m_scrubbing){
        return;
    }

    m_scrubbing = true;

    if(m_playing){
        stop_running();
    }
}

void playback_controller::end_scrub(){
    if(!m_scrubbing){
        return;
    }

    m_scrubbing = false;

    if(m_playing){
        start_running();
    }
}

void playback_controller::start_running(){
    m_clock.play(now());

    // A sample right at the start time plays too.
    m_samples_done_until = m_time - 1;

    sync_song();
    m_tick.start();
}

void playback_controller::stop_running(){
    m_tick.stop();
    m_mixer->stop_all();
    m_clock.pause(now());
    set_time(m_clock.time(now()));
    sync_song();
}

void playback_controller::sync_song(){
    if(!m_song_ready){
        return;
    }

    const bool wanted = running() && !m_song_finished && m_time < m_song_length;
    const bool playing = m_player->playbackState() == QMediaPlayer::PlayingState;

    if(wanted && !playing){
        m_player->setPosition(m_clock.time(now()));
        m_player->play();
    }
    else if(!wanted && playing){
        m_player->pause();
    }
}

void playback_controller::tick(){
    if(!running()){
        return;
    }

    const qint64 t = m_clock.time(now());
    set_time(t);
    sync_song();

    for(const qsizetype index : m_schedule.due(m_samples_done_until, t)){
        const QString& file = m_sample_files.at(index);

        if(!file.isEmpty()){
            m_mixer->play(m_bank->sound(file), float(m_samples.at(index).volume) / 100.0f);
        }

        emit sampleTriggered(index);
    }

    m_samples_done_until = std::max(m_samples_done_until, t);
}

void playback_controller::set_time(qint64 ms){
    if(ms == m_time){
        return;
    }

    m_time = ms;
    emit timeChanged(ms);
}

void playback_controller::set_music_volume(float volume){
    m_music_volume = std::clamp(volume, 0.0f, 1.0f);
    apply_gains();
}

void playback_controller::set_sample_volume(float volume){
    m_sample_volume = std::clamp(volume, 0.0f, 1.0f);
    apply_gains();
}

void playback_controller::set_muted(bool muted){
    m_muted = muted;
    apply_gains();
}

void playback_controller::set_rate(double rate){
    rate = std::clamp(rate, 0.1, 2.0);

    if(rate == m_clock.rate()){
        return;
    }

    m_clock.set_rate(rate, now());
    m_player->setPlaybackRate(rate);
}

void playback_controller::apply_gains(){
    m_music_output->setVolume(m_music_volume);
    m_music_output->setMuted(m_muted);
    m_mixer->set_master_gain(m_muted ? 0.0f : m_sample_volume);
}
