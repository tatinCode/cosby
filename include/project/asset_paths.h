#pragma once

#include <QString>
#include <QStringList>

// The project folder that holds storyboard images. Beatmap updates replace
// beatmap/ but never touch it.
inline constexpr const char* assets_folder_name = "assets";

// "sb\\star.png" and "./sb/star.png" both become "sb/star.png", the form
// written to the .osb.
QString normalize_asset_path(const QString& path);

// True for a relative path that stays inside its folder: not empty, not
// absolute, no "..". Only these are looked up, copied or exported as-is.
bool is_safe_asset_path(const QString& path);

// The first folder that has the file, as an absolute path; empty when none
// has it or the path is not safe.
QString find_asset(const QStringList& folders, const QString& path);

// Where the preview and export look for images, in order: the project's
// assets/, then the beatmap copy. Folders that are empty strings are skipped.
QStringList asset_folders(const QString& project_root, const QString& beatmap_folder);
