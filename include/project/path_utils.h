#pragma once

#include <QString>

/**
 * Returns true if child_path is parent_path itself or is located inside it.
 * Both paths should already be canonical (QFileInfo::canonicalFilePath()).
 */
bool same_or_child_path(const QString& parent_path, const QString& child_path);
