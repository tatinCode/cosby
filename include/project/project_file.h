#pragma once

#include <QString>
#include <optional>

struct ProjectData{
    int format_version = 1;
    QString beatmap_file;
    QString script_file = "scripts/main.js";

    /**
     * Optional. Absolute path of the folder the beatmap snapshot was copied
     * from. Only used as a hint when updating the snapshot; it may no longer
     * exist (e.g. osu!lazer's temporary external-edit mount).
     */
    QString source_directory;
};

bool save_project_file(
        const QString& path,
        const ProjectData& project,
        QString* error = nullptr
        );

std::optional<ProjectData> load_project_file(
        const QString& path,
        QString* error = nullptr
        );


