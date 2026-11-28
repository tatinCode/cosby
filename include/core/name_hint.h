#pragma once

#include <QString>
#include <QStringList>

/**
 * The candidate `input` was most likely meant to be, for "Did you mean ...?"
 * hints: the closest by edit distance, ignoring case, if it's close enough
 * to be a typo (at most a third of the input's length, and at least 1). A
 * swap of neighbouring letters counts as one change. Empty when nothing is
 * that close; ties go to the earlier candidate.
 */
QString closest_name(const QString& input, const QStringList& candidates);

// ` Did you mean "name"?`, or empty when there's no close candidate.
QString did_you_mean(const QString& input, const QStringList& candidates);
