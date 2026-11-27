#pragma once

#include "core/scene.h"

#include <QList>
#include <QString>

// Something in the scene osu! would show differently than the script asks
// for. The export still goes through.
struct osb_warning{
    QString message;
    int line = 0;       // script line of the sprite (0 when unknown)
};

/**
 * The scene as an osu! storyboard (.osb): sprites per layer in osu!'s order
 * (Background, Fail, Pass, Foreground, Overlay), in creation order within a
 * layer, each followed by its commands.
 *
 * Numbers use '.' and at most 4 decimals; colours are whole 0-255 values.
 * Flip/additive that last for good are written with end = start, which is
 * how osu! spells "permanent".
 */
QString write_osb(const scene& sc, QList<osb_warning>* warnings = nullptr);

// osu!'s spelling of a layer or origin ("Centre", not "Center").
QString osb_layer_name(layer_t layer);
QString osb_origin_name(origin_t origin);

// A number as written to the .osb: rounded to 4 decimals, no trailing zeros.
QString osb_number(double value);
