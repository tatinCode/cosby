#pragma once

#include "core/scene.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

/**
 * The storyboard file osu! loads for every difficulty of a map:
 * "Artist - Title (Creator) [Hard].osu" -> "Artist - Title (Creator).osb".
 */
QString osb_file_name(const QString& osu_file_path);

// An image the export copies into the beatmap folder.
struct asset_copy{
    QString path;           // relative, as used by the script ("sb/star.png")
    QString source;         // absolute
    QString destination;    // absolute
    bool replaces = false;  // a different file is there already
};

struct export_plan{
    QString target_folder;
    QString osb_path;
    bool osb_replaces = false;

    // Other .osb files in the folder; osu!lazer may load one of those instead.
    QStringList other_osb_files;

    QList<asset_copy> copies;

    // Images found nowhere: not in assets/, the beatmap folder or its copy.
    QStringList missing;

    // True when anything already in the folder would be overwritten.
    bool replaces_files() const;
};

/**
 * Works out what exporting the scene to target_folder would write, without
 * writing anything.
 *
 * Images are taken from assets_folder (the project's assets/) and copied when
 * the beatmap folder lacks them or has a different version. An image that is
 * only in the beatmap copy is copied back if the beatmap folder lost it, but
 * never replaces the beatmap folder's own file. Unsafe paths (absolute, "..")
 * are skipped; write_osb() warns about them.
 *
 * Fails when target_folder doesn't exist or lies inside project_root (that
 * would export into the project, e.g. its beatmap copy, not into osu!).
 */
std::optional<export_plan> plan_export(
        const scene& sc,
        const QString& target_folder,
        const QString& osb_name,
        const QString& project_root,
        const QString& assets_folder,
        const QString& beatmap_copy_folder,
        QString* error = nullptr
        );

// Copies the images, then writes the .osb (each file replaced atomically).
bool apply_export(const export_plan& plan, const QString& osb_text, QString* error = nullptr);
