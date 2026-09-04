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
            void test_rejects_osu_outside_source();
            void test_rejects_non_empty_project();
            void test_rejects_overlapping_directories();
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

void TestProjectCreator::test_rejects_osu_outside_source(){
    QTemporaryDir temporary_directory;

    QVERIFY(temporary_directory.isValid());

    const QString source_directory = temporary_directory.filePath("source");

    const QString project_directory = temporary_directory.filePath("project");
    
    const QString outside_osu_file = temporary_directory.filePath("outside.osu");

    QVERIFY(QDir().mkpath(source_directory));
    QVERIFY(QDir().mkpath(project_directory));

    QVERIFY(write_test_file(
                outside_osu_file,
                "osu file format v14\n"
                ));

    QString error;

    const auto project_file = create_project_from_directory(
            source_directory,
            outside_osu_file,
            project_directory,
            &error
            );

    QVERIFY(!project_file.has_value());
    QVERIFY(error.contains("inside"));

    const QDir project(project_directory);

    QVERIFY(!QFileInfo::exists(project.filePath("project.cosby")));
    QVERIFY(!QFileInfo::exists(project.filePath("scripts")));
    QVERIFY(!QFileInfo::exists(project.filePath("beatmap")));

}

void TestProjectCreator::test_rejects_non_empty_project(){
    QTemporaryDir temporary_directory;

    QVERIFY(temporary_directory.isValid());

    const QString source_directory = temporary_directory.filePath("source");

    const QString project_directory = temporary_directory.filePath("project");

    const QString osu_file = QDir(source_directory).filePath("difficulty.osu");

    const QString existing_file = QDir(project_directory).filePath("keep.txt");

    QVERIFY(write_test_file(
                osu_file,
                "osu file format v14\n"
                ));

    QVERIFY(write_test_file(
                existing_file,
                "keep this file"
                ));

    QString error;

    const auto project_file = create_project_from_directory(
            source_directory,
            osu_file,
            project_directory,
            &error
            );

    QVERIFY(!project_file.has_value());
    QVERIFY(error.contains("not empty"));

    QCOMPARE(
            read_test_file(existing_file),
            QByteArray("keep this file")
            );

    const QDir project(project_directory);

    QVERIFY(!QFileInfo::exists(project.filePath("project.cosby")));
    QVERIFY(!QFileInfo::exists(project.filePath("scripts")));
    QVERIFY(!QFileInfo::exists(project.filePath("beatmap")));
}

void TestProjectCreator::test_rejects_overlapping_directories(){
    QTemporaryDir temporary_directory;

    QVERIFY(temporary_directory.isValid());

    //Project directory inside the beatmap source
    const QString first_source = temporary_directory.filePath("first-source");

    const QString first_project = QDir(first_source).filePath("project");

    const QString first_osu = QDir(first_source).filePath("difficulty.osu");

    QVERIFY(write_test_file(
                first_osu,
                "osu file format v14\n"
                ));

    QVERIFY(QDir().mkpath(first_project));

    QString error;

    const auto first_result = create_project_from_directory(
            first_source,
            first_osu,
            first_project,
            &error
            );

    QVERIFY(!first_result.has_value());
    QVERIFY(error.contains("separate"));
    QVERIFY(!QFileInfo::exists(
                QDir(first_project).filePath("project.cosby")
                ));

    //Beatmap source inside the project directory
    const QString second_project = temporary_directory.filePath("second-project");

    const QString second_source = QDir(second_project).filePath("source");

    const QString second_osu = QDir(second_source).filePath("difficulty.osu");

    QVERIFY(write_test_file(
                second_osu,
                "osu file format v14\n"
                ));

    const auto second_result = create_project_from_directory(
            second_source,
            second_osu,
            second_project,
            &error
            );

    QVERIFY(!second_result.has_value());
    QVERIFY(error.contains("separate"));

    const QDir project(second_project);

    QVERIFY(!QFileInfo::exists(project.filePath("project.cosby")));
    QVERIFY(!QFileInfo::exists(project.filePath("scripts")));
    QVERIFY(!QFileInfo::exists(project.filePath("beatmap")));
}

QTEST_APPLESS_MAIN(TestProjectCreator)

#include "test_project_creator.moc"









