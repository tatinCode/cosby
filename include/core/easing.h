#pragma once

#include <QString>
#include <QStringList>

// osu!'s easings. The values are the numbers written to the .osb, so the
// order must never change.
enum class easing_t : int {
    linear, out, in,
    quad_in, quad_out, quad_in_out,
    cubic_in, cubic_out, cubic_in_out,
    quart_in, quart_out, quart_in_out,
    quint_in, quint_out, quint_in_out,
    sine_in, sine_out, sine_in_out,
    expo_in, expo_out, expo_in_out,
    circ_in, circ_out, circ_in_out,
    elastic_in, elastic_out, elastic_half_out, elastic_quarter_out, elastic_in_out,
    back_in, back_out, back_in_out,
    bounce_in, bounce_out, bounce_in_out,
};

inline constexpr int easing_count = int(easing_t::bounce_in_out) + 1;

// The names scripts use, as on the osu! wiki ("QuadOut", "ElasticHalfOut").
QString easing_name(easing_t easing);

// Every name, in osu!'s order.
QStringList easing_names();

/**
 * Accepts a name in any case ("quadout"), or osu!'s number as text ("4").
 * Returns false for anything else; `out` is left alone then.
 */
bool parse_easing(const QString& text, easing_t& out);

// Eased progress for t in [0, 1], exactly as osu! computes it. 0 and 1 map
// to exactly 0 and 1; values in between can overshoot (Back, Elastic).
double apply_easing(easing_t easing, double t);
