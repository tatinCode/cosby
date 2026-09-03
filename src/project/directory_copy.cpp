#include "project/directory_copy.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDirIterator>

namespace{
    void set_error(QString* error, const QString& message){
        if(error){
            *error = message;
        }
    }

    bool fail_copy(
            const QString& destination_path, 
            QString* error,
            const QString& message
            ){
        /**
         * The destination did not exit before copying, so its safe to remove
         * files created by a failed copy.
         */
        QDir(destination_path).removeRecursively();
        set_error(error, message);

        return false;
    }

}

bool copy_directory(
        const QString& source_path,
        const QString& destination_path,
        QString* error
        ){

    if(error){
        error->clear();
    }

    const QFileInfo source_info(source_path);

    if(!source_info.exists() || !source_info.isDir()){
        set_error(error, "Source beatmap directory does not exist");

        return false;
    }

    if(source_info.isSymLink()){
        set_error(error, "Source beatmap directory cannot be a symlink");

        return false;
    }

    const QString source = QDir::cleanPath(source_info.absoluteFilePath());

    const QString destination = QDir::cleanPath(QFileInfo(destination_path).absoluteFilePath());

#ifdef Q_OS_WIN
    const Qt::CaseSensitivity path_case = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity path_case = Qt::CaseSensitive;
#endif

    const QString source_prefix = source.endsWith('/') ? source : source + '/';

    if(destination.compare(source, path_case) == 0 ||
            destination.startsWith(source_prefix, path_case)){

            set_error(error, "Source and destination cannot be the same");

            return false;
    }

    if(QFileInfo::exists(destination)){
        set_error(error, "Destination directory already exists");

        return false;
    }

    if(!QDir().mkpath(destination)){
        set_error(error, "Could not create destination directory");

        return false;
    }

    const QDir source_directory(source);
    const QDir destination_directory(destination);

    QDirIterator iterator(
            source,
            QDir::AllEntries |
            QDir::NoDotAndDotDot |
            QDir::Hidden |
            QDir::System,
            QDirIterator::Subdirectories
            );

    while(iterator.hasNext()){
        iterator.next();

        const QFileInfo entry = iterator.fileInfo();

        if(entry.isSymLink()){
            return fail_copy(
                    destination,
                    error,
                    QString("Symbolic links are not supported: %1").arg(entry.filePath())
                    );
        }

        const QString relative_path = source_directory.relativeFilePath(entry.filePath());

        const QString target_path = destination_directory.filePath(relative_path);

        if(entry.isDir()){
            if(!QDir().mkpath(target_path)){
                return fail_copy(
                        destination,
                        error,
                        QString("Could not create directory: %1").arg(target_path)
                        );
            }

            continue;
        }

        if(!entry.isFile()){
            continue;
        }

        const QString target_parent = QFileInfo(target_path).absolutePath();

        if(!QDir().mkpath(target_parent)){
            return fail_copy(
                    destination,
                    error,
                    QString("Could not create parent directory: %1").arg(relative_path)
                    );
        }

        if(!QFile::copy(entry.filePath(), target_path)){
            return fail_copy(
                    destination,
                    error,
                    QString("Could not copy file: %1").arg(relative_path)
                    );
        }
    }
    
    return true;
}




