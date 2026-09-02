#include "project/project_file.h"

#include <QTest>
#include <QTemporaryDir>
#include <QFile>

namespace{
    bool write_test_file(const QString& path, const QByteArray& contents){
        QFile file(path);

        if(!file.open(QIODevice::WriteOnly)){
            return false;
        }

        return file.write(contents) == contents.size();
    }
}

class TestProjectFile : public QObject{
    Q_OBJECT

    private slots:
        void test_round_trip();
        void test_invalid_json();
        void test_unsupported_version();
        void test_missing_beatmap_file();
        void test_missing_script_file();
        void test_empty_save_values();

};

void TestProjectFile::test_round_trip(){
    QTemporaryDir dir;

    const QString path = dir.filePath("project.cosby");

    ProjectData expected;
    expected.format_version = 1;
    expected.beatmap_file = "/example/beatmap/difficulty.osu";
    expected.script_file = "example/scripts/main.js";

    QString error;

    QVERIFY2(save_project_file(path, expected, &error), qPrintable(error));

    const auto actual = load_project_file(path, &error);

    QVERIFY2(actual.has_value(), qPrintable(error));
    QCOMPARE(actual->format_version, expected.format_version);
    QCOMPARE(actual->beatmap_file, expected.beatmap_file);
    QCOMPARE(actual->script_file, expected.script_file);
}

void TestProjectFile::test_invalid_json(){
    QTemporaryDir dir;

    const QString path = directory.filePath("project.cosby");

    QVERIFY2(write_test_file(path, "{ invalid json }"));

    QString error;

    const auto project = load_project_file(path, &error);

    QVERIFY(!project.has_value());
    QVERIFY(!error.isEmpty());
}

void TestProjectFile::test_unsupported_version(){
    QTermporaryDir dir;

    QVERIFY(directory.isValid());

    const QString path = directory.filePath("project.cosby");

    const QByteArray json = R"({
        "formatVersion": 2,
        "beatmapFile": "/example/difficulty.osu",
        "scriptFile": "/scripts/main.js"
        })";

    QVERIFY(write_test_file(path, json));

    QString error;
    const auto project = load_project_file(path, &error);

    QVERIFY(!project.has_value());
    QVERIFY(error.contains("Unsupported"));
}

void TestProjectFile::test_missing_beatmap_file(){
    QTemporaryDir dir;
    
    QVERIFY(directory.isValid());

    const QString path = directory.filePath("project.cosby");

    const QByteArray json = R"({
        "formatVersion": 1,
        "scriptFile": "/scripts/main.js"
    })";

    QVERIFY(write_test_file(path, json));

    QString error;

    const auto project = load_project_file(path, &error);

    QVERIFY(!project.has_value());
    QVERIFY(error.contains("beatmapFile"));
}

void TestProjectFile::test_missing_script_file(){
    QTemporaryDir dir;

    QVERIFY(directory.isValid());

    cons QString path = directory.filePath("project.cosby");

    const QByteArray json = R"({
        "formatVersion": 1,
        "beatmapFile": "/example/difficulty.osu"
    })";

    QVERIFY(write_test_file(path, json));

    QString error;
    
    const auto project = load_project_file(path, &error);

    QVERIFY(!project.has_value());
    QVERIFY(error.contains("scriptsFile"));
}

void TestProjectFile::test_empty_save_values(){
    QTemporaryDir dir;

    QVERIFY(directory.isValid());

    const QString path = directory.filePath("project.cosby");

    ProjectData project;
    QString error;

    project.beatmap_file.clear();

    QVERIFY(!save_project_file(path, project, &error));
    QVERIFY(!error.isEmpty());

    project.beatmap_file = "/example/difficulty.osu";
    project.script_file.clear();

    QVERIFY(!save_project_file(path, project, &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(!save_project_file(path, project, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(TestProjectFile)

#include "test_project_file.moc"





