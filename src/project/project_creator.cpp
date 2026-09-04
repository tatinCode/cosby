#include "project/project_creator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace{
    void set_error(QString* error, const QString& message){
        if(error){
            *error = message;
        }
    }

    bool same_or_child_path(const QString& parent_path, const QString& child_path){
        QString parent = QDir::fromNativeSeparators(QDir::cleanPath(parent_path));
        QString child = QDir::fromNativeSeparators(QDir::cleanPath(child_path));

#ifdef Q_OS_WIN
        const Qt::CaseSensitivity path_case = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity path_case = Qt::CaseSensitive;
#endif

        if(child.compare(parent, path_case) == 0){
            return true;
        }

        if(!parent.endsWith('/')){
            parent += '/';
        }

        return child.startsWith(parent, path_case);
    }

    void clean_created_files(const QString& project_directory){
        const QDir project(project_directory);

        QFile::remove(project.filePath("project.cosby"));
        QDir(project.filePath("scripts")).removeRecursively();
        QDir(project.filePath("beatmap")).removeRecursively();
    }

    bool write_initial_script(const QString& path, QString* error){
        QSaveFile file(path);

        if(!file.open(QIODevice::WriteOnly | QIODevice::Text)){
            set_error(
                    error, 
                    QString("Could not create main script: %1")
                    .arg(file.errorString())
                    );

            return false;
        }

        const QByteArray contents = 
            "// Create your storyboard here\n";

        if(file.write(contents) != contents.size()){
            set_error(
                    error, 
                    QString("Could not write main script: %1")
                    .arg(file.errorString())
                    );

            file.cancelWriting();

            return false;
        }

        if(!file.commit()){
            set_error(
                    error, 
                    QString("Could not commit main script: %1")
                    .arg(file.errorString())
                    );

            return false;
        }

        return true;
    }
}

std::optional<QString> create_project_from_directory(
        const QString& source_directory,
        const QString& selected_osu_file,
        const QString& project_directory,
        QString* error
        ){
    if(error){
        error->clear();
    }

    const QFileInfo source_info(source_directory);

    if(!source_info.exists() || !source_info.isDir()){
        set_error(error, "Source beatmap directory does not exist");

        return std::nullopt;
    }

    const QFileInfo osu_info(selected_osu_file);

    if(!osu_info.exists() || !osu_info.isFile()){
        set_error(error, "Selected .osu file does not exist");

        return std::nullopt;
    }

    if(osu_info.suffix().compare("osu", Qt::CaseInsensitive) != 0){
        set_error(error, "Selected beatmap file must use the .osu extension");

        return std::nullopt;
    }

    const QFileInfo project_info(project_directory);

    if(!project_info.exists() || !project_info.isDir()){
        set_error(error, "Project directory cannot be a symbolic link");

        return std::nullopt;
    }

    const QString source = source_info.canonicalFilePath();
    const QString osu_file = osu_info.canonicalFilePath();
    const QString project_root = project_info.canonicalFilePath();

    if(!same_or_child_path(source, project_root)){
        set_error(
                error,
                "Selected .osu file must be inside the source beatmap directory"
                );

        return std::nullopt;
    }

    if(same_or_child_path(osu_file, project_root) ||
            same_or_child_path(project_root, osu_file)){
        set_error(
                error,
                "Project and beatmap file cannot be the same"
                );

        return std::nullopt;
    }

    QDir project(project_root);

    const QStringList existing_entries = project.entryList(
            QDir::AllEntries |
            QDir::NoDotAndDotDot |
            QDir::Hidden |
            QDir::System
            );

    if(!existing_entries.isEmpty()){
        set_error(error, "Project directory is not empty");

        return std::nullopt;
    }

    const QString beatmap_destination = project.filePath("beatmap");

    if(!copy_directory(source, beatmap_destination, error)){
        return std::nullopt;
    }

    const QString relative_osu_file = QDir(source).relativeFilePath(osu_file);

    const QString copied_osu_file = QDir(beatmap_destination).filePath(relative_osu_file);

    if(!QFileInfo::exists(copied_osu_file)){
        clean_created_files(project_root);
        set_error(error, "Copied .osu file could not be found");

        return std::nullopt;
    }
    
    if(!project.mkpath("scripts")){
        clean_created_files(project_root);
        set_error(error, "Could not create scripts directory");

        return std::nullopt;
    }

    const QString script_relative_path = "scripts/main.js";

    const QString script_path = project.filePath(script_relative_path);

    if(!write_initial_script(script_path, error)){
        clean_created_files(project_root);

        return std::nullopt;
    }

    ProjectData project_data;
    project_data.beatmap_file = project.relativeFilePath(relative_osu_file);
    project_data.script_file = script_relative_path;

    const QString project_file_path = project.filePathb("project.cosby");

    if(!save_project_file(
                project_file_path,
                project_data,
                error
                )){
        clean_created_files(project_root);

        return std::nullopt;
    }
    
    return project_file_path;
}


