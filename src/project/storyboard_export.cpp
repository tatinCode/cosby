#include "project/storyboard_export.h"
#include "project/asset_paths.h"
#include "project/path_utils.h"
#include "core/sprite_timing.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace {
    void set_error(QString* error, const QString& message){
        if(error){
            *error = message;
        }
    }

    std::optional<QByteArray> read_file(const QString& path){
        QFile file(path);

        if(!file.open(QIODevice::ReadOnly)){
            return std::nullopt;
        }

        return file.readAll();
    }

    bool same_contents(const QString& a, const QString& b){
        if(QFileInfo(a).size() != QFileInfo(b).size()){
            return false;
        }

        const auto first = read_file(a);
        const auto second = read_file(b);

        return first && second && *first == *second;
    }

    bool write_file(const QString& path, const QByteArray& data, QString* error){
        QSaveFile file(path);

        if(!file.open(QIODevice::WriteOnly)){
            set_error(error, QString("Could not write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString()));
            return false;
        }

        if(file.write(data) != data.size() || !file.commit()){
            set_error(error, QString("Could not write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString()));
            return false;
        }

        return true;
    }
}

QString osb_file_name(const QString& osu_file_path){
    QString name = QFileInfo(osu_file_path).completeBaseName();

    // Drop the trailing " [Difficulty]".
    static const QRegularExpression difficulty(R"(\s*\[[^\]]*\]$)");
    name.remove(difficulty);

    return name + ".osb";
}

QStringList used_files(const scene& sc){
    QStringList files;

    for(const sprite& s : sc.sprites){
        files << image_paths(s);
    }

    for(const sample& smp : sc.samples){
        files << smp.path;
    }

    return files;
}

bool export_plan::replaces_files() const{
    if(osb_replaces){
        return true;
    }

    return std::any_of(copies.begin(), copies.end(), [](const asset_copy& c){ return c.replaces; });
}

std::optional<export_plan> plan_export(
        const scene& sc,
        const QString& target_folder,
        const QString& osb_name,
        const QString& project_root,
        const QString& assets_folder,
        const QString& beatmap_copy_folder,
        QString* error){

    const QFileInfo target_info(target_folder);

    if(!target_info.isDir()){
        set_error(error, QString("The beatmap folder does not exist:\n%1").arg(QDir::toNativeSeparators(target_folder)));
        return std::nullopt;
    }

    const QString target = target_info.canonicalFilePath();
    const QString project = QFileInfo(project_root).canonicalFilePath();

    if(!project.isEmpty() && same_or_child_path(project, target)){
        set_error(error, "Export to the beatmap folder in osu!, not into the project");
        return std::nullopt;
    }

    if(osb_name.isEmpty() || osb_name.contains('/') || osb_name.contains('\\')){
        set_error(error, QString("Invalid storyboard file name \"%1\"").arg(osb_name));
        return std::nullopt;
    }

    const QDir target_dir(target);

    export_plan plan;
    plan.target_folder = target;
    plan.osb_path = target_dir.filePath(osb_name);
    plan.osb_replaces = QFileInfo::exists(plan.osb_path);

    for(const QString& other : target_dir.entryList({"*.osb"}, QDir::Files, QDir::Name)){
        if(other != osb_name){
            plan.other_osb_files << other;
        }
    }

    QSet<QString> seen;

    for(const QString& used : used_files(sc)){
        const QString path = normalize_asset_path(used);

        if(seen.contains(path) || !is_safe_asset_path(path)){
            continue;
        }

        seen.insert(path);

        const QString destination = target_dir.filePath(path);
        const bool in_target = QFileInfo(destination).isFile();
        const QString from_assets = find_asset({assets_folder}, path);

        if(!from_assets.isEmpty()){
            if(!in_target || !same_contents(from_assets, destination)){
                plan.copies.append({path, from_assets, destination, in_target});
            }
            continue;
        }

        if(in_target){
            continue;       // the beatmap's own file
        }

        const QString from_copy = find_asset({beatmap_copy_folder}, path);

        if(!from_copy.isEmpty()){
            plan.copies.append({path, from_copy, destination, false});
        }
        else{
            plan.missing << path;
        }
    }

    return plan;
}

bool apply_export(const export_plan& plan, const QString& osb_text, QString* error){
    // Images first: a failed copy leaves the old .osb in place, never a new
    // one pointing at images that aren't there.
    for(const asset_copy& copy : plan.copies){
        const auto data = read_file(copy.source);

        if(!data){
            set_error(error, QString("Could not read %1").arg(QDir::toNativeSeparators(copy.source)));
            return false;
        }

        if(!QDir().mkpath(QFileInfo(copy.destination).absolutePath())){
            set_error(error, QString("Could not create the folder for %1").arg(copy.path));
            return false;
        }

        if(!write_file(copy.destination, *data, error)){
            return false;
        }
    }

    return write_file(plan.osb_path, osb_text.toUtf8(), error);
}
