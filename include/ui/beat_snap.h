#pragma once

#include <QColor>
#include <QtGlobal>

// Beat snap colours, as in the osu! editor: each tick of the timeline's grid
// is coloured by the simplest division of a beat it falls on.

// The simplest division of a beat that tick `tick` lands on, on a grid of
// `subdivision` ticks per beat: 1 on a beat, 2 halfway, 4 on the quarters,
// and so on (3 for the thirds of a 1/6 grid). Negative ticks work too.
int beat_division(qint64 tick, int subdivision);

// The colour of a division: white beats, red 1/2, blue 1/4, yellow 1/8,
// purple 1/3, ...; grey for divisions osu! has no colour for.
QColor beat_division_color(int division);
