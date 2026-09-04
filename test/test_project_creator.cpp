#include "project/project_creator.h"
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
}

class TestProjectCreator : public QObject{
    Q_OBJECT

        private slots:
            void test_creates_project_from_directory();
};

void TestProjectCreator::test_creates_project_from_directory(){
    QTemporaryDir temporary_directory;

    QVERIFY(temporary_directory.isValid());

    const QString source_directory = temporary_directory.filePath("source");

    const QString project_directory = temporary_directory.filePath("project");

    const QString osu_file = QDir(source_directory).filePath("difficulty.osu");

    QVERIFY(write_test_file(
                osu_file,
                "osu file format v14\n"
                ));

    QVERIFY(write_test_file(
                QDir(source_directory).filePath("audio.mp3"),
                "audio contents"
                ));

    QVERIFY(write_test_file(
                QDir(source_directory).filePath("nested/background.jpg"),
                "background contents"
                ));

    QVERIFY(write_test_file(
                QDir(source_directory).filePath(".hidden"),
                "hidden contents"
                ));

    QVERIFY(QDir().mkpath(project_directory));

    QString error;

    const auto project_file = create_project_from_directory(
            source_directory,
            osu_file,
            project_directory,
            &error
            );

    QVERIFY2(project_file.has_value(), qPrintable(error));

    const QString expected_project_file = QDir(project_directory).filePath("project.cosby");

    QCOMPARE(
            QFileInfo(*project_file).absoluteFilePath(),
            QFileInfo(expected_project_file).absoluteFilePath()
            );

    QVERIFY(QFileInfo::exists(expected_project_file));

    const QDir project(project_directory);

    const QString copied_osu_file = project.filePath("beatmap/difficulty.osu");

    const QString copied_audio_file = project.filePath("beatmap/audio.mp3");

    const QString copied_background_file = project.filePath("beatmap/nested/background.jpg");

    const QString copied_hidden_file = project.filePath("beatmap/.hidden");

    const QString script_file = project.filePath("scripts/main.js");

    QVERIFY(QFileInfo::exists(copied_osu_file));
    QVERIFY(QFileInfo::exists(copied_audio_file));
    QVERIFY(QFileInfo::exists(copied_background_file));
    QVERIFY(QFileInfo::exists(copied_hidden_file));
    QVERIFY(QFileInfo::exists(script_file));

    QCOMPARE(
            read_test_file(copied_audio_file),
            QByteArray("audio contents")
            );

    QCOMPARE(
            read_test_file(copied_background_file),
            QByteArray("background contents")
            );

    const auto project_data = load_project_file(expected_project_file, &error);

    QVERIFY2(project_data.has_value(), qPrintable(error));

    QCOMPARE(
            project_data->beatmap_file,
            QString("beatmap/difficulty.osu")
            );

    QCOMPARE(
            project_data->script_file,
            QString("scripts/main.js")
            );
}

QTEST_APPLESS_MAIN(TestProjectCreator)

#include "test_project_creator.moc"









