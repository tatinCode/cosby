#pragma once

#include <optional>
#include <QString>

std::optional<QString> create_project_from_directory(
        const QString& source_directory,
        const QString& selected_osu_file,
        const QString& project_directory,
        QString* error
        );

