#include "project/directory_copy.h"

#include <QtTest>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

namespace{
    bool write_test_file(const QString& path, const QByteArray& contents){
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

class TestDirectoryCopy : public QObject{
    Q_OBJECT

        private slots:
            void test_copies_nested_files();
            void test_missing_source();
            void test_existing_destination();
            void test_destination_inside_source();
};

void TestDirectoryCopy::test_copies_nested_files(){
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());

    const QString source = temporary_directory.filePath("source");

    const QString destination = temporary_directory.filePath("destination");

    QVERIFY(write_test_file(
                QDir(source).filePath("root.txt"),
                    "root contents"
                    ));

    QVERIFY(write_test_file(
                QDir(source).filePath("nested/file.txt"),
                    "nested contents"
                    ));

    QVERIFY(write_test_file(
                QDir(source).filePath(".hidden"),
                    "hidden contents"
                    ));

    QString error;

    QVERIFY2(copy_directory(source, destination, &error),
            qPrintable(error)
            );

    QCOMPARE(read_test_file(QDir(destination).filePath("root.txt")),
            QByteArray("root contents")
            );

    QCOMPARE(read_test_file(QDir(destination).filePath("nested/file.txt")),
            QByteArray("nested contents")
            );

    QCOMPARE(read_test_file(QDir(destination).filePath(".hidden")),
            QByteArray("hidden contents")
            );
}

void TestDirectoryCopy::test_missing_source(){
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());

    const QString source = temporary_directory.filePath("missing");

    const QString destination = temporary_directory.filePath("destination");

    QString error;

    QVERIFY(!copy_directory(source, destination, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(destination));
}

void TestDirectoryCopy::test_existing_destination(){
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());

    const QString source = temporary_directory.filePath("source");

    const QString destination = temporary_directory.filePath("destination");

    QVERIFY(QDir().mkpath(source));
    QVERIFY(QDir().mkpath(destination));

    QString error;

    QVERIFY(!copy_directory(source, destination, &error));
    QVERIFY(error.contains("already exists"));
}

void TestDirectoryCopy::test_destination_inside_source(){
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());

    const QString source = temporary_directory.filePath("source");

    const QString destination = QDir(source).filePath("copied");

    QVERIFY(QDir().mkpath(source));

    QString error;

    QVERIFY(!copy_directory(source, destination, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(destination));
}

QTEST_APPLESS_MAIN(TestDirectoryCopy)

#include "test_directory_copy.moc"





