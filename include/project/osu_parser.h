#pragma once

#include <QString>

struct BeatmapImportInfo{
    QString beatmap_folder;
    QString osu_file_path;
    QString audio_file;
    QString background_file;

    double bpm = 180.0;

    // Time of the first uninherited timing point: where the beat grid starts.
    double beat_offset_ms = 0.0;

    bool is_valid() const{
        return !beatmap_folder.isEmpty() && !osu_file_path.isEmpty();
    }
};

BeatmapImportInfo parse_beatmap_metadata(const QString& osu_file_path);
