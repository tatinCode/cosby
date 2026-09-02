#pragma once

#include <QString>

struct BeatmapImportInfo{
    QString beatmap_folder;
    QString osu_file_path;
    QString audio_file;
    QString background_file;

    double bpm = 180.0;

    bool is_valid() const{
        return !beatmap_folder.isEmpty() && !osu_file_path.isEmpty();
    }
};

BeatmapImportInfo parse_beatmap_metadata(const QString& osu_file_path);
