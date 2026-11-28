#include "project/asset_check.h"
#include "project/asset_paths.h"
#include "core/sprite_timing.h"

#include <QSet>

namespace {
    const QString unsafe_message =
        QStringLiteral("%1: the path must be relative to the beatmap folder, without \"..\"");

    QString not_found(const QString& path, const char* what){
        return QStringLiteral("%1: %2 not found in %3/ or beatmap/")
            .arg(path, QString::fromLatin1(what), QString::fromLatin1(assets_folder_name));
    }
}

QList<asset_problem> check_assets(const scene& sc, const QStringList& folders){
    QList<asset_problem> problems;
    QSet<QString> checked;

    // True the first time `path` is seen, so each file is reported once.
    auto first_time = [&checked](const QString& path){
        if(checked.contains(path)){
            return false;
        }

        checked.insert(path);
        return true;
    };

    for(const sprite& s : sc.sprites){
        const QString path = normalize_asset_path(s.path);

        if(!first_time(path)){
            continue;
        }

        if(!is_safe_asset_path(path)){
            problems.append({unsafe_message.arg(s.path), s.source_line});
            continue;
        }

        if(!s.is_animation()){
            if(find_asset(folders, path).isEmpty()){
                problems.append({not_found(path, "image"), s.source_line});
            }
            continue;
        }

        QStringList missing;

        for(const QString& frame : image_paths(s)){
            if(find_asset(folders, frame).isEmpty()){
                missing << frame;
            }
        }

        if(missing.size() == 1 && s.frame_count == 1){
            problems.append({not_found(missing.front(), "animation frame"), s.source_line});
        }
        else if(missing.size() == s.frame_count){
            problems.append({QStringLiteral("%1: none of the %2 animation frames found (%3 ... %4) in %5/ or beatmap/")
                    .arg(path).arg(s.frame_count).arg(missing.front(), missing.back(),
                         QString::fromLatin1(assets_folder_name)), s.source_line});
        }
        else if(!missing.isEmpty()){
            problems.append({QStringLiteral("%1: %2 of %3 animation frames not found: %4")
                    .arg(path).arg(missing.size()).arg(s.frame_count).arg(missing.join(QStringLiteral(", "))),
                    s.source_line});
        }
    }

    for(const sample& smp : sc.samples){
        const QString path = normalize_asset_path(smp.path);

        if(!first_time(path)){
            continue;
        }

        if(!is_safe_asset_path(path)){
            problems.append({unsafe_message.arg(smp.path), smp.source_line});
        }
        else if(find_asset(folders, path).isEmpty()){
            problems.append({not_found(path, "sound"), smp.source_line});
        }
    }

    return problems;
}
