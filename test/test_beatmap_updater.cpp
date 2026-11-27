#include "project/beatmap_updater.h"
#include "cosby_test.h"
#include "project/project_creator.h"
#include "project/osu_parser.h"
#include "project/project_file.h"

#include <QTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace{
    bool write_test_file(
            const QString& path,
            const QByteArray& contents
            ){
        if(!QDir().mkpath(QFileInfo(path).absolutePath())){
            return false;
        }

        QFile file(path);

        if(!file.open(QIODevice::WriteOnly)){
            return false;
        }

        return file.write(contents) == contents.size();
    }

    QByteArray read_test_file(const QString& path){
        QFile file(path);

        if(!file.open(QIODevice::ReadOnly)){
            return {};
        }

        return file.readAll();
    }

    /**
     * Creates <root>/source with difficulty.osu, audio.mp3 and old.jpg, and a
     * project in <root>/project. Returns the project.cosby path.
     */
    QString create_test_project(const QTemporaryDir& root, QString* error){
        const QDir source(root.filePath("source"));

        if(!write_test_file(source.filePath("difficulty.osu"), "old timing") ||
                !write_test_file(source.filePath("audio.mp3"), "audio") ||
                !write_test_file(source.filePath("old.jpg"), "old image") ||
                !QDir().mkpath(root.filePath("project"))){
            *error = "Could not create test files";

            return {};
        }

        const auto project_file = create_project_from_directory(
                source.path(),
                source.filePath("difficulty.osu"),
                root.filePath("project"),
                error
                );

        return project_file.value_or(QString());
    }

    void verify_no_leftovers(const QDir& project){
        QVERIFY(!QFileInfo::exists(project.filePath("beatmap.new")));
        QVERIFY(!QFileInfo::exists(project.filePath("beatmap.old")));
    }
}

class TestBeatmapUpdater : public QObject{
    Q_OBJECT

        private slots:
            void test_replaces_snapshot();
            void test_follows_renamed_difficulty();
            void test_records_new_source_directory();
            void test_missing_source_keeps_snapshot();
            void test_osu_outside_source_keeps_snapshot();
            void test_rejects_source_inside_project();
            void test_failed_copy_keeps_snapshot();
            void test_recovers_interrupted_update();
};

void TestBeatmapUpdater::test_replaces_snapshot(){
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QString error;
    const QString project_file = create_test_project(root, &error);
    QVERIFY2(!project_file.isEmpty(), qPrintable(error));

    const QDir source(root.filePath("source"));
    const QDir project(root.filePath("project"));

    // The user edits the script before updating.
    QVERIFY(write_test_file(project.filePath("scripts/main.js"), "my script"));

    // The mapper changes timing, adds a file and deletes one.
    QVERIFY(write_test_file(source.filePath("difficulty.osu"), "new timing"));
    QVERIFY(write_test_file(source.filePath("nested/new.png"), "new image"));
    QVERIFY(QFile::remove(source.filePath("old.jpg")));

    const auto beatmap_file = update_beatmap_snapshot(
            project_file,
            source.path(),
            source.filePath("difficulty.osu"),
            &error
            );

    QVERIFY2(beatmap_file.has_value(), qPrintable(error));
    QCOMPARE(*beatmap_file, QString("beatmap/difficulty.osu"));

    QCOMPARE(read_test_file(project.filePath("beatmap/difficulty.osu")), QByteArray("new timing"));
    QCOMPARE(read_test_file(project.filePath("beatmap/nested/new.png")), QByteArray("new image"));
    QCOMPARE(read_test_file(project.filePath("beatmap/audio.mp3")), QByteArray("audio"));
    QVERIFY(!QFileInfo::exists(project.filePath("beatmap/old.jpg")));

    QCOMPARE(read_test_file(project.filePath("scripts/main.js")), QByteArray("my script"));

    verify_no_leftovers(project);

    const auto data = load_project_file(project_file, &error);
    QVERIFY2(data.has_value(), qPrintable(error));
    QCOMPARE(data->beatmap_file, QString("beatmap/difficulty.osu"));
    QCOMPARE(data->script_file, QString("scripts/main.js"));
}

void TestBeatmapUpdater::test_follows_renamed_difficulty(){
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QString error;
    const QString project_file = create_test_project(root, &error);
    QVERIFY2(!project_file.isEmpty(), qPrintable(error));

    const QDir source(root.filePath("source"));
    const QDir project(root.filePath("project"));

    QVERIFY(QFile::rename(source.filePath("difficulty.osu"), source.filePath("renamed.osu")));

    const auto beatmap_file = update_beatmap_snapshot(
            project_file,
            source.path(),
            source.filePath("renamed.osu"),
            &error
            );

    QVERIFY2(beatmap_file.has_value(), qPrintable(error));
    QCOMPARE(*beatmap_file, QString("beatmap/renamed.osu"));
    QVERIFY(QFileInfo::exists(project.filePath("beatmap/renamed.osu")));
    QVERIFY(!QFileInfo::exists(project.filePath("beatmap/difficulty.osu")));

    const auto data = load_project_file(project_file, &error);
    QVERIFY2(data.has_value(), qPrintable(error));
    QCOMPARE(data->beatmap_file, QString("beatmap/renamed.osu"));
}

void TestBeatmapUpdater::test_records_new_source_directory(){
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QString error;
    const QString project_file = create_test_project(root, &error);
    QVERIFY2(!project_file.isEmpty(), qPrintable(error));

    auto data = load_project_file(project_file, &error);
    QVERIFY2(data.has_value(), qPrintable(error));
    QCOMPARE(data->source_directory, QFileInfo(root.filePath("source")).canonicalFilePath());

    // e.g. a new lazer external-edit mount at a different path
    const QDir other_source(root.filePath("other-source"));
    QVERIFY(write_test_file(other_source.filePath("difficulty.osu"), "other"));

    const auto beatmap_file = update_beatmap_snapshot(
            project_file,
            other_source.path(),
            other_source.filePath("difficulty.osu"),
            &error
            );

    QVERIFY2(beatmap_file.has_value(), qPrintable(error));

    data = load_project_file(project_file, &error);
    QVERIFY2(data.has_value(), qPrintable(error));
    QCOMPARE(data->source_directory, QFileInfo(other_source.path()).canonicalFilePath());
}

void TestBeatmapUpdater::test_missing_source_keeps_snapshot(){
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QString error;
    const QString project_file = create_test_project(root, &error);
    QVERIFY2(!project_file.isEmpty(), qPrintable(error));

    const QDir project(root.filePath("project"));
    const QString missing = root.filePath("missing");

    const auto beatmap_file = update_beatmap_snapshot(
            project_file,
            missing,
            QDir(missing).filePath("difficulty.osu"),
            &error
            );

    QVERIFY(!beatmap_file.has_value());
    QVERIFY(!error.isEmpty());
    QCOMPARE(read_test_file(project.filePath("beatmap/difficulty.osu")), QByteArray("old timing"));
    verify_no_leftovers(project);
}

void TestBeatmapUpdater::test_osu_outside_source_keeps_snapshot(){
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QString error;
    const QString project_file = create_test_project(root, &error);
    QVERIFY2(!project_file.isEmpty(), qPrintable(error));

    const QDir project(root.filePath("project"));
    const QString outside_osu = root.filePath("outside.osu");
    QVERIFY(write_test_file(outside_osu, "outside"));

    const auto beatmap_file = update_beatmap_snapshot(
            project_file,
            root.filePath("source"),
            outside_osu,
            &error
            );

    QVERIFY(!beatmap_file.has_value());
    QVERIFY(error.contains("inside"));
    QCOMPARE(read_test_file(project.filePath("beatmap/difficulty.osu")), QByteArray("old timing"));
    verify_no_leftovers(project);
}

void TestBeatmapUpdater::test_rejects_source_inside_project(){
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QString error;
    const QString project_file = create_test_project(root, &error);
    QVERIFY2(!project_file.isEmpty(), qPrintable(error));

    const QDir project(root.filePath("project"));

    // Picking the project's own snapshot as the source must be rejected.
    const auto beatmap_file = update_beatmap_snapshot(
            project_file,
            project.filePath("beatmap"),
            project.filePath("beatmap/difficulty.osu"),
            &error
            );

    QVERIFY(!beatmap_file.has_value());
    QVERIFY(error.contains("separate"));
    QCOMPARE(read_test_file(project.filePath("beatmap/difficulty.osu")), QByteArray("old timing"));
    verify_no_leftovers(project);
}

void TestBeatmapUpdater::test_failed_copy_keeps_snapshot(){
#ifdef Q_OS_WIN
    QSKIP("Symbolic links require extra privileges on Windows");
#endif

    QTemporaryDir root;
    QVERIFY(root.isValid());

    QString error;
    const QString project_file = create_test_project(root, &error);
    QVERIFY2(!project_file.isEmpty(), qPrintable(error));

    const QDir source(root.filePath("source"));
    const QDir project(root.filePath("project"));

    // copy_directory() rejects symbolic links, so the copy fails midway.
    QVERIFY(write_test_file(source.filePath("difficulty.osu"), "new timing"));
    QVERIFY(QFile::link(source.filePath("audio.mp3"), source.filePath("link.mp3")));

    const auto beatmap_file = update_beatmap_snapshot(
            project_file,
            source.path(),
            source.filePath("difficulty.osu"),
            &error
            );

    QVERIFY(!beatmap_file.has_value());
    QVERIFY(!error.isEmpty());
    QCOMPARE(read_test_file(project.filePath("beatmap/difficulty.osu")), QByteArray("old timing"));
    verify_no_leftovers(project);
}

void TestBeatmapUpdater::test_recovers_interrupted_update(){
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QString error;
    const QString project_file = create_test_project(root, &error);
    QVERIFY2(!project_file.isEmpty(), qPrintable(error));

    const QDir source(root.filePath("source"));
    QDir project(root.filePath("project"));

    // Simulate a crash between "beatmap -> beatmap.old" and "beatmap.new -> beatmap".
    QVERIFY(project.rename("beatmap", "beatmap.old"));
    QVERIFY(write_test_file(project.filePath("beatmap.new/partial.txt"), "partial"));

    QVERIFY(write_test_file(source.filePath("difficulty.osu"), "new timing"));

    const auto beatmap_file = update_beatmap_snapshot(
            project_file,
            source.path(),
            source.filePath("difficulty.osu"),
            &error
            );

    QVERIFY2(beatmap_file.has_value(), qPrintable(error));
    QCOMPARE(read_test_file(project.filePath("beatmap/difficulty.osu")), QByteArray("new timing"));
    QVERIFY(!QFileInfo::exists(project.filePath("beatmap/partial.txt")));
    verify_no_leftovers(project);
}

COSBY_TEST(TestBeatmapUpdater)

namespace {
    QString write_osu(const QTemporaryDir& dir, const QString& timing_points){
        const QString path = dir.filePath("map.osu");
        QFile file(path);

        if(!file.open(QIODevice::WriteOnly | QIODevice::Text)){
            return {};
        }

        file.write(QString(
            "osu file format v14\n"
            "\n"
            "[General]\n"
            "AudioFilename: audio.mp3\n"
            "\n"
            "[Events]\n"
            "0,0,\"bg.jpg\",0,0\n"
            "\n"
            "[TimingPoints]\n"
            "%1\n"
            "\n"
            "[HitObjects]\n").arg(timing_points).toUtf8());

        return path;
    }
}

// The .osu metadata the timeline needs (BPM, beat offset).
class TestOsuParser : public QObject {
    Q_OBJECT

    private slots:
        void reads_bpm_and_beat_offset(){
            QTemporaryDir dir;
            QVERIFY(dir.isValid());

            const auto info = parse_beatmap_metadata(write_osu(dir,
                        "1234,333.333333333333,4,2,0,100,1,0\n"
                        "5000,-100,4,2,0,100,0,0"));

            QCOMPARE(qRound(info.bpm), 180);
            QCOMPARE(info.beat_offset_ms, 1234.0);
            QCOMPARE(info.audio_file, QString("audio.mp3"));
            QCOMPARE(info.background_file, QString("bg.jpg"));
        }

        void skips_inherited_points_before_the_first_red_line(){
            QTemporaryDir dir;
            QVERIFY(dir.isValid());

            const auto info = parse_beatmap_metadata(write_osu(dir,
                        "-50,-100,4,2,0,100,0,0\n"
                        "820,500,4,2,0,100,1,0"));

            QCOMPARE(info.bpm, 120.0);
            QCOMPARE(info.beat_offset_ms, 820.0);
        }

        void offset_defaults_to_zero_without_timing_points(){
            QTemporaryDir dir;
            QVERIFY(dir.isValid());

            const auto info = parse_beatmap_metadata(write_osu(dir, QString()));

            QCOMPARE(info.beat_offset_ms, 0.0);
            QCOMPARE(info.bpm, 180.0);
        }
};

COSBY_TEST(TestOsuParser)

#include "test_beatmap_updater.moc"
