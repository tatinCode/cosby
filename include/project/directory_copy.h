#pragma once

#include <QString>

bool copy_directory(
        const QString& source_path,
        const QString& destination_path,
        QString* error = nullptr
        );


