#pragma once

#include "core/scene.h"

#include <QList>
#include <QString>
#include <QStringList>

// A file the storyboard uses that the preview (and export) can't find.
struct asset_problem{
    QString message;        // e.g. "sb/star.png: not found in assets/ or beatmap/"
    int source_line = 0;    // the script line that uses it; 0 when unknown
};

/**
 * Checks every image, animation frame and sound the scene uses against
 * `folders` (see asset_folders()). One problem per file, reported at the
 * first line that uses it; an animation's missing frames are reported
 * together. Paths that aren't relative or contain ".." are reported as such.
 * A missing image or sound names a similar file next to it if there is one
 * ("Did you mean "sb/Star.png"?"): another case, a small typo or another
 * extension. Case matters because osu! on Linux, macOS and lazer is
 * case-sensitive.
 */
QList<asset_problem> check_assets(const scene& sc, const QStringList& folders);
