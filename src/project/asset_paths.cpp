#include "project/asset_paths.h"

#include <QDir>
#include <QFileInfo>

QString normalize_asset_path(const QString& path){
    QString normalized = path.trimmed();
    normalized.replace('\\', '/');

    while(normalized.startsWith("./")){
        normalized.remove(0, 2);
    }

    return normalized;
}

bool is_safe_asset_path(const QString& path){
    const QString normalized = normalize_asset_path(path);

    if(normalized.isEmpty() || QDir::isAbsolutePath(normalized) || normalized.startsWith('/')){
        return false;
    }

    // "C:foo" is relative to a drive on Windows, not to the folder.
    if(normalized.size() >= 2 && normalized.at(1) == ':'){
        return false;
    }

    const QStringList parts = normalized.split('/');

    return !parts.contains("..");
}

QString find_asset(const QStringList& folders, const QString& path){
    if(!is_safe_asset_path(path)){
        return {};
    }

    const QString relative = normalize_asset_path(path);

    for(const QString& folder : folders){
        const QFileInfo file(QDir(folder).filePath(relative));

        if(file.isFile()){
            return file.absoluteFilePath();
        }
    }

    return {};
}

QStringList asset_folders(const QString& project_root, const QString& beatmap_folder){
    QStringList folders;

    if(!project_root.isEmpty()){
        folders << QDir(project_root).filePath(assets_folder_name);
    }

    if(!beatmap_folder.isEmpty()){
        folders << beatmap_folder;
    }

    return folders;
}
