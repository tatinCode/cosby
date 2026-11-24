#pragma once

#include <optional>
#include <QString>

/**
 * Replaces the project's beatmap/ snapshot with a fresh copy of
 * source_directory, then updates beatmapFile and sourceDirectory in
 * project.cosby. The script and every other project file are untouched.
 *
 * The new copy is staged in beatmap.new/ and swapped in with renames, so on
 * failure the previous snapshot is kept.
 *
 * Returns the new relative beatmapFile (e.g. "beatmap/difficulty.osu").
 */
std::optional<QString> update_beatmap_snapshot(
        const QString& project_file_path,
        const QString& source_directory,
        const QString& selected_osu_file,
        QString* error = nullptr
        );
