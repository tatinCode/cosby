#include "audio/playback_clock.h"
#include "audio/playback_controller.h"
#include "audio/sample_bank.h"
#include "audio/sample_mixer.h"
#include "audio/sample_schedule.h"
#include "cosby_test.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QDataStream>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

// Playback: the clock that follows the song, which samples are due, the
// mixer, decoding, and the controller tying them together.

class TestPlaybackClock : public QObject{
    Q_OBJECT

    private slots:
        void stays_put_while_paused(){
            playback_clock clock;
            QCOMPARE(clock.time(0), 0);
            QCOMPARE(clock.time(5000), 0);

            clock.seek(1200, 5000);
            QCOMPARE(clock.time(9000), 1200);
        }

        void runs_on_wall_time_without_a_song(){
            playback_clock clock;
            clock.seek(1000, 0);
            clock.play(100);

            QCOMPARE(clock.time(100), 1000);
            QCOMPARE(clock.time(350), 1250);

            clock.pause(600);
            QCOMPARE(clock.time(5000), 1500);

            clock.play(6000);                   // resumes where it paused
            QCOMPARE(clock.time(6100), 1600);
        }

        void seeking_while_playing_keeps_playing(){
            playback_clock clock;
            clock.play(0);
            clock.seek(5000, 100);

            QVERIFY(clock.is_playing());
            QCOMPARE(clock.time(100), 5000);
            QCOMPARE(clock.time(300), 5200);
        }

        void seeking_back_moves_the_time_back(){
            playback_clock clock;
            clock.play(0);
            QCOMPARE(clock.time(2000), 2000);

            clock.seek(500, 2000);
            QCOMPARE(clock.time(2000), 500);
        }

        void rate_scales_the_time_and_keeps_the_position(){
            playback_clock clock;
            clock.play(0);
            QCOMPARE(clock.time(1000), 1000);

            clock.set_rate(0.5, 1000);
            QCOMPARE(clock.time(1000), 1000);
            QCOMPARE(clock.time(2000), 1500);

            clock.pause(2000);
            clock.set_rate(0.25, 3000);          // while paused: no jump
            QCOMPARE(clock.time(3000), 1500);

            clock.set_rate(0, 3000);             // ignored
            QCOMPARE(clock.rate(), 0.25);
        }

        void small_drift_is_corrected_gradually(){
            playback_clock clock;
            clock.play(0);

            // The song is 40 ms ahead: each report closes part of the gap.
            clock.song_position(1040, 1000);
            const qint64 first = clock.time(1000);
            QVERIFY(first > 1000 && first < 1040);

            for(int i = 1; i <= 40; ++i){
                clock.song_position(1040 + i * 10, 1000 + i * 10);
            }

            QVERIFY(std::abs(clock.time(1400) - 1440) <= 1);
        }

        void a_song_behind_holds_the_picture_instead_of_going_back(){
            playback_clock clock;
            clock.play(0);
            QCOMPARE(clock.time(1000), 1000);

            // 200 ms behind: snapped, but the time shown doesn't go back...
            clock.song_position(800, 1000);
            QCOMPARE(clock.time(1000), 1000);
            QCOMPARE(clock.time(1100), 1000);

            // ...it waits for the song to catch up, then runs with it.
            QCOMPARE(clock.time(1300), 1100);
        }

        void a_big_jump_ahead_is_followed_at_once(){
            playback_clock clock;
            clock.play(0);
            clock.song_position(3000, 1000);
            QCOMPARE(clock.time(1000), 3000);
        }

        void a_song_far_behind_moves_the_time_back(){
            playback_clock clock;
            clock.play(0);
            QCOMPARE(clock.time(5000), 5000);

            clock.song_position(1000, 5000);
            QCOMPARE(clock.time(5000), 1000);
        }

        void reports_while_paused_are_ignored(){
            playback_clock clock;
            clock.seek(2000, 0);
            clock.song_position(9000, 10);
            QCOMPARE(clock.time(20), 2000);
        }
};

class TestSampleSchedule : public QObject{
    Q_OBJECT

    private:
        static sample at(int time, layer_t layer = layer_t::foreground){
            sample s;
            s.time = time;
            s.layer = layer;
            s.path = "a.wav";
            return s;
        }

    private slots:
        void due_is_after_exclusive_up_to_inclusive_in_time_order(){
            const sample_schedule schedule({at(300), at(100), at(200), at(100)});

            QCOMPARE(schedule.due(0, 99), QVector<qsizetype>{});
            QCOMPARE(schedule.due(0, 100), (QVector<qsizetype>{1, 3}));     // ties: script order
            QCOMPARE(schedule.due(100, 300), (QVector<qsizetype>{2, 0}));
            QCOMPARE(schedule.due(300, 1000), QVector<qsizetype>{});
        }

        void fail_and_overlay_samples_dont_play(){
            const sample_schedule schedule({at(10, layer_t::fail), at(20, layer_t::pass),
                                            at(30, layer_t::background), at(40, layer_t::overlay)});

            QCOMPARE(schedule.due(0, 100), (QVector<qsizetype>{1, 2}));
            QVERIFY(sample_schedule(QVector<sample>{at(10, layer_t::fail)}).is_empty());
        }

        void a_late_tick_drops_old_samples(){
            const sample_schedule schedule({at(100), at(900), at(1000)});

            // The app stalled from 0 to 1000: only the last 150 ms still play.
            QCOMPARE(schedule.due(0, 1000), (QVector<qsizetype>{1, 2}));
            QCOMPARE(schedule.due(0, 1000 + sample_schedule::late_limit_ms), (QVector<qsizetype>{2}));
        }

        void an_empty_or_backwards_interval_is_empty(){
            const sample_schedule schedule({at(100)});
            QVERIFY(schedule.due(100, 100).isEmpty());
            QVERIFY(schedule.due(200, 50).isEmpty());
        }
};

class TestSampleMixer : public QObject{
    Q_OBJECT

    private:
        static sound_ptr sound(std::initializer_list<float> values){
            return std::make_shared<const QVector<float>>(values);
        }

    private slots:
        void mixes_voices_with_their_gains(){
            std::vector<mixer_voice> voices{
                {sound({0.5f, 0.5f, 0.5f, 0.5f}), 0, 1.0f},
                {sound({0.2f, -0.2f, 0.2f, -0.2f}), 0, 0.5f},
            };

            float out[4] = {9, 9, 9, 9};
            mix_voices(voices, out, 2, 1.0f);

            QCOMPARE(out[0], 0.6f);
            QCOMPARE(out[1], 0.4f);
            QCOMPARE(out[3], 0.4f);
            QVERIFY(voices.empty());            // both ended
        }

        void continues_voices_across_calls_and_pads_with_silence(){
            std::vector<mixer_voice> voices{{sound({0.1f, 0.1f, 0.2f, 0.2f, 0.3f, 0.3f}), 0, 1.0f}};

            float out[4];
            mix_voices(voices, out, 2, 1.0f);
            QCOMPARE(out[2], 0.2f);
            QCOMPARE(voices.size(), size_t(1));

            mix_voices(voices, out, 2, 1.0f);
            QCOMPARE(out[0], 0.3f);
            QCOMPARE(out[2], 0.0f);
            QVERIFY(voices.empty());
        }

        void clamps_and_applies_the_master_gain(){
            std::vector<mixer_voice> voices{
                {sound({0.8f, -0.8f}), 0, 1.0f},
                {sound({0.8f, -0.8f}), 0, 1.0f},
            };

            float out[2];
            mix_voices(voices, out, 1, 1.0f);
            QCOMPARE(out[0], 1.0f);
            QCOMPARE(out[1], -1.0f);

            std::vector<mixer_voice> quiet{{sound({0.8f, 0.8f}), 0, 1.0f}};
            mix_voices(quiet, out, 1, 0.0f);
            QCOMPARE(out[0], 0.0f);
        }
};

namespace {
    // A mono 16-bit WAV of `ms` milliseconds of a 440 Hz tone.
    bool write_wav(const QString& path, int rate, int ms){
        const int frames = rate * ms / 1000;

        QByteArray pcm;
        QDataStream pcm_out(&pcm, QIODevice::WriteOnly);
        pcm_out.setByteOrder(QDataStream::LittleEndian);

        for(int i = 0; i < frames; ++i){
            pcm_out << qint16(std::sin(i * 2 * M_PI * 440 / rate) * 16000);
        }

        QFile file(path);
        if(!file.open(QIODevice::WriteOnly)){
            return false;
        }

        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData("RIFF", 4);
        out << quint32(36 + pcm.size());
        out.writeRawData("WAVEfmt ", 8);
        out << quint32(16) << quint16(1) << quint16(1) << quint32(rate)
            << quint32(rate * 2) << quint16(2) << quint16(16);
        out.writeRawData("data", 4);
        out << quint32(pcm.size());
        out.writeRawData(pcm.constData(), int(pcm.size()));

        return out.status() == QDataStream::Ok;
    }

    QAudioFormat stereo_float(int rate){
        QAudioFormat format;
        format.setSampleRate(rate);
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Float);
        return format;
    }
}

class TestSampleBank : public QObject{
    Q_OBJECT

    private slots:
        void mono_buffers_become_stereo(){
            QAudioFormat mono;
            mono.setSampleRate(8000);
            mono.setChannelCount(1);
            mono.setSampleFormat(QAudioFormat::Int16);

            const qint16 data[] = {16384, -16384};
            const QAudioBuffer buffer(QByteArray(reinterpret_cast<const char*>(data), sizeof data), mono);

            QVector<float> out;
            append_as_stereo(buffer, out);

            // Int16 is normalized by 32767, so 16384 is a hair over 0.5.
            const float expected[] = {0.5f, 0.5f, -0.5f, -0.5f};
            QCOMPARE(out.size(), qsizetype(4));
            for(int i = 0; i < 4; ++i){
                QVERIFY2(std::abs(out[i] - expected[i]) < 1e-3f, qPrintable(QString::number(out[i])));
            }
        }

        void resampling_keeps_the_duration(){
            QVector<float> in;
            for(int i = 0; i < 100; ++i){
                in << float(i) << float(-i);
            }

            const QVector<float> up = resample_stereo(in, 24000, 48000);
            QCOMPARE(up.size(), qsizetype(400));
            QCOMPARE(up[2], 0.5f);              // halfway between frames 0 and 1
            QCOMPARE(up[3], -0.5f);

            QCOMPARE(resample_stereo(in, 48000, 48000), in);
            QCOMPARE(resample_stereo(in, 48000, 24000).size(), qsizetype(100));
        }

        void decodes_files_and_reloads_changed_ones(){
            if(!QAudioDecoder().isSupported()){
                QSKIP("No audio decoder backend");
            }

            QTemporaryDir dir;
            const QString path = dir.filePath("hit.wav");
            QVERIFY(write_wav(path, 22050, 100));

            sample_bank bank(stereo_float(44100));
            QSignalSpy idle(&bank, &sample_bank::idle);
            QSignalSpy failed(&bank, &sample_bank::failed);

            bank.load({path, dir.filePath("missing.wav")});
            QVERIFY(idle.wait(5000) || idle.count() > 0);
            QCOMPARE(failed.count(), 0);

            const sound_ptr sound = bank.sound(path);
            QVERIFY(sound);
            // 100 ms at 44.1 kHz, stereo (decoders may pad a little).
            QVERIFY2(std::abs(sound->size() - 8820) < 400, qPrintable(QString::number(sound->size())));
            QVERIFY(!bank.sound(dir.filePath("missing.wav")));

            // Unchanged: nothing to decode, the same sound stays.
            idle.clear();
            bank.load({path});
            QCOMPARE(idle.count(), 1);
            QCOMPARE(bank.sound(path), sound);

            // No longer used: dropped.
            bank.load({});
            QVERIFY(!bank.sound(path));
        }

        void reports_files_it_cant_decode(){
            if(!QAudioDecoder().isSupported()){
                QSKIP("No audio decoder backend");
            }

            QTemporaryDir dir;
            const QString path = dir.filePath("broken.wav");
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("definitely not audio");
            file.close();

            sample_bank bank(stereo_float(48000));
            QSignalSpy failed(&bank, &sample_bank::failed);

            bank.load({path});
            QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
            QCOMPARE(failed.first().first().toString(), path);
            QVERIFY(!bank.sound(path));
        }
};

class TestPlaybackController : public QObject{
    Q_OBJECT

    private:
        qint64 m_now = 0;

        std::unique_ptr<playback_controller> make(){
            m_now = 0;
            return std::make_unique<playback_controller>(nullptr, [this]{ return m_now; });
        }

        static sample at(int time, layer_t layer = layer_t::foreground){
            sample s;
            s.time = time;
            s.layer = layer;
            s.path = "none.wav";
            return s;
        }

        static QVector<qsizetype> indices(const QSignalSpy& spy){
            QVector<qsizetype> out;
            for(const auto& args : spy){
                out << args.first().value<qsizetype>();
            }
            return out;
        }

    private slots:
        void plays_and_pauses_without_a_song(){
            auto playback = make();
            QSignalSpy playing(playback.get(), &playback_controller::playingChanged);
            QSignalSpy times(playback.get(), &playback_controller::timeChanged);

            playback->play();
            QVERIFY(playback->is_playing());
            QCOMPARE(playing.count(), 1);

            m_now = 500;
            playback->tick();
            QCOMPARE(playback->time(), 500);
            QCOMPARE(times.last().first().toLongLong(), 500);

            playback->toggle_playback();
            QVERIFY(!playback->is_playing());
            m_now = 2000;
            playback->tick();
            QCOMPARE(playback->time(), 500);

            playback->stop();
            QCOMPARE(playback->time(), 0);
            QCOMPARE(times.last().first().toLongLong(), 0);
        }

        void seeking_reports_the_time_at_once(){
            auto playback = make();
            QSignalSpy times(playback.get(), &playback_controller::timeChanged);

            playback->seek(1234);
            QCOMPARE(times.count(), 1);
            QCOMPARE(playback->time(), 1234);

            playback->seek(-50);
            QCOMPARE(playback->time(), 0);
        }

        void scrubbing_pauses_time_but_not_the_play_state(){
            auto playback = make();
            QSignalSpy playing(playback.get(), &playback_controller::playingChanged);

            playback->play();
            m_now = 100;
            playback->tick();

            playback->begin_scrub();
            playback->seek(5000);
            m_now = 900;
            playback->tick();                    // still where the drag put it
            QCOMPARE(playback->time(), 5000);
            QVERIFY(playback->is_playing());     // the button doesn't flicker
            QCOMPARE(playing.count(), 1);

            playback->end_scrub();
            m_now = 1000;
            playback->tick();
            QCOMPARE(playback->time(), 5100);
        }

        void scrubbing_while_paused_stays_paused(){
            auto playback = make();
            playback->begin_scrub();
            playback->seek(300);
            playback->end_scrub();

            m_now = 1000;
            playback->tick();
            QVERIFY(!playback->is_playing());
            QCOMPARE(playback->time(), 300);
        }

        void samples_go_off_as_the_time_passes_them(){
            auto playback = make();
            QSignalSpy triggered(playback.get(), &playback_controller::sampleTriggered);

            playback->set_samples({at(0), at(100), at(150), at(400, layer_t::fail), at(500)}, {});
            playback->play();                    // a sample right at the start plays

            m_now = 120;
            playback->tick();
            QCOMPARE(indices(triggered), (QVector<qsizetype>{0, 1}));

            m_now = 300;
            playback->tick();
            m_now = 600;
            playback->tick();                    // the Fail one stays silent
            QCOMPARE(indices(triggered), (QVector<qsizetype>{0, 1, 2, 4}));
        }

        void seeking_skips_samples(){
            auto playback = make();
            QSignalSpy triggered(playback.get(), &playback_controller::sampleTriggered);

            playback->set_samples({at(100), at(1000), at(1100)}, {});
            playback->play();
            playback->seek(1000);                // over the one at 100, onto the one at 1000

            m_now = 50;
            playback->tick();
            QVERIFY(triggered.isEmpty());

            m_now = 150;
            playback->tick();
            QCOMPARE(indices(triggered), (QVector<qsizetype>{2}));
        }

        void running_the_script_again_doesnt_replay_samples(){
            auto playback = make();
            QSignalSpy triggered(playback.get(), &playback_controller::sampleTriggered);

            playback->set_samples({at(100)}, {});
            playback->play();
            m_now = 200;
            playback->tick();
            QCOMPARE(triggered.count(), 1);

            playback->set_samples({at(100), at(150), at(300)}, {});
            m_now = 400;
            playback->tick();
            QCOMPARE(indices(triggered), (QVector<qsizetype>{0, 2}));
        }

        void rate_slows_the_time(){
            auto playback = make();
            playback->set_rate(0.5);
            playback->play();

            m_now = 1000;
            playback->tick();
            QCOMPARE(playback->time(), 500);
            QCOMPARE(playback->rate(), 0.5);
        }

        void a_missing_song_is_reported(){
            QTemporaryDir dir;
            auto playback = make();
            QSignalSpy problems(playback.get(), &playback_controller::songProblem);

            playback->set_song(dir.filePath("audio.mp3"));
            QCOMPARE(problems.count(), 1);
            QVERIFY(problems.first().first().toString().contains("audio.mp3"));
            QVERIFY(!playback->song_ready());

            // Still plays, silently.
            playback->play();
            m_now = 250;
            playback->tick();
            QCOMPARE(playback->time(), 250);

            playback->set_song({});
            QCOMPARE(problems.count(), 1);
        }

        void a_song_sets_its_length_once_loaded(){
            if(!QAudioDecoder().isSupported()){
                QSKIP("No multimedia backend");
            }

            QTemporaryDir dir;
            const QString path = dir.filePath("song.wav");
            QVERIFY(write_wav(path, 22050, 1500));

            auto playback = make();
            playback->set_song(path);

            QTRY_VERIFY_WITH_TIMEOUT(playback->song_ready(), 5000);
            QVERIFY(std::abs(playback->song_length() - 1500) < 50);
        }
};

COSBY_TEST(TestPlaybackClock)
COSBY_TEST(TestSampleSchedule)
COSBY_TEST(TestSampleMixer)
COSBY_TEST(TestSampleBank)
COSBY_TEST(TestPlaybackController)
#include "test_playback.moc"
