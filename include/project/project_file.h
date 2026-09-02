#pragma once

#include <QString>
#include <optional>

struct ProjectData{
    int format_version = 1;
    QString beatmap_file;
    QString script_file = "scripts/main.js";
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


