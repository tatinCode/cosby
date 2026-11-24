#include "project/beatmap_updater.h"

#include "project/directory_copy.h"
#include "project/path_utils.h"
#include "project/project_file.h"

#include <QDir>
#include <QFileInfo>

namespace{
    const QString beatmap_folder = "beatmap";
    const QString staging_folder = "beatmap.new";
    const QString backup_folder = "beatmap.old";

    void set_error(QString* error, const QString& message){
        if(error){
            *error = message;
        }
    }

    /**
     * Cleans up leftovers from an update that was interrupted (e.g. a crash).
     * beatmap.new/ is always an incomplete copy. beatmap.old/ is only needed
     * if the swap stopped before beatmap/ was put back.
     */
    bool recover_interrupted_update(QDir& project, QString* error){
        if(project.exists(staging_folder) &&
                !QDir(project.filePath(staging_folder)).removeRecursively()){
            set_error(error, "Could not remove leftover beatmap.new directory");

            return false;
        }

        if(!project.exists(backup_folder)){
            return true;
        }

        if(project.exists(beatmap_folder)){
            if(!QDir(project.filePath(backup_folder)).removeRecursively()){
                set_error(error, "Could not remove leftover beatmap.old directory");

                return false;
            }

            return true;
        }

        if(!project.rename(backup_folder, beatmap_folder)){
            set_error(error, "Could not restore beatmap directory from beatmap.old");

            return false;
        }

        return true;
    }
}

std::optional<QString> update_beatmap_snapshot(
        const QString& project_file_path,
        const QString& source_directory,
        const QString& selected_osu_file,
        QString* error
        ){
    if(error){
        error->clear();
    }

    auto project_data = load_project_file(project_file_path, error);

    if(!project_data.has_value()){
        return std::nullopt;
    }

    const QFileInfo source_info(source_directory);

    if(!source_info.exists() || !source_info.isDir()){
        set_error(error, "Source beatmap directory does not exist");

        return std::nullopt;
    }

    if(source_info.isSymLink()){
        set_error(error, "Source beatmap directory cannot be a symbolic link");

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

    const QString source = source_info.canonicalFilePath();
    const QString osu_file = osu_info.canonicalFilePath();
    const QString project_root =
        QFileInfo(QFileInfo(project_file_path).absolutePath()).canonicalFilePath();

    if(!same_or_child_path(source, osu_file)){
        set_error(
                error,
                "Selected .osu file must be inside the source beatmap directory"
                );

        return std::nullopt;
    }

    if(same_or_child_path(source, project_root) ||
            same_or_child_path(project_root, source)){
        set_error(
                error,
                "Project and beatmap directories must be separate"
                );

        return std::nullopt;
    }

    QDir project(project_root);

    if(!recover_interrupted_update(project, error)){
        return std::nullopt;
    }

    const QString staging_path = project.filePath(staging_folder);

    // 1. Copy the new version next to the current snapshot.
    if(!copy_directory(source, staging_path, error)){
        return std::nullopt;
    }

    // 2. Make sure the selected difficulty made it into the copy.
    const QString relative_osu_file = QDir(source).relativeFilePath(osu_file);

    if(!QFileInfo::exists(QDir(staging_path).filePath(relative_osu_file))){
        QDir(staging_path).removeRecursively();
        set_error(error, "Copied .osu file could not be found");

        return std::nullopt;
    }

    // 3. Move the current snapshot out of the way.
    const bool had_snapshot = project.exists(beatmap_folder);

    if(had_snapshot && !project.rename(beatmap_folder, backup_folder)){
        QDir(staging_path).removeRecursively();
        set_error(error, "Could not move the current beatmap snapshot aside");

        return std::nullopt;
    }

    const auto restore_previous_snapshot = [&](){
        if(had_snapshot){
            project.rename(backup_folder, beatmap_folder);
        }
    };

    // 4. Swap the new copy in.
    if(!project.rename(staging_folder, beatmap_folder)){
        QDir(staging_path).removeRecursively();
        restore_previous_snapshot();
        set_error(error, "Could not move the new beatmap snapshot into place");

        return std::nullopt;
    }

    // 5. Point project.cosby at the (possibly renamed) difficulty.
    const QString new_beatmap_file = QDir::fromNativeSeparators(
            QDir::cleanPath(beatmap_folder + '/' + relative_osu_file)
            );

    project_data->beatmap_file = new_beatmap_file;
    project_data->source_directory = source;

    if(!save_project_file(project_file_path, *project_data, error)){
        QDir(project.filePath(beatmap_folder)).removeRecursively();
        restore_previous_snapshot();

        return std::nullopt;
    }

    // 6. The update is committed; drop the old snapshot. A failure here only
    //    leaves beatmap.old/ behind, which the next update cleans up.
    if(had_snapshot){
        QDir(project.filePath(backup_folder)).removeRecursively();
    }

    return new_beatmap_file;
}
