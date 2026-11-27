#pragma once

#include <QString>
#include <QtGlobal>

#include <optional>

// Playhead time as text, and back. No widget code.
// osu! measures everything in whole milliseconds; the editor just shows them
// as minutes:seconds:milliseconds.

// "01:23:456", the timestamp format of the osu! editor.
QString format_osu_timestamp(qint64 ms);

// Parses what a user types into the time field:
//   83456          milliseconds (the unit scripts use)
//   1:23.456  1:23 minutes:seconds[.fraction]
//   01:23:456      osu! editor timestamp; a pasted "01:23:456 (1,2) - " works too
//   12.5s          seconds
//   +500  -1:00    relative to `current_ms`
// Returns nullopt for anything else, or for times beyond 24 hours.
std::optional<qint64> parse_time_input(const QString& text, qint64 current_ms);
