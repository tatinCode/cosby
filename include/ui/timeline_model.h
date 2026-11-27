#pragma once

#include "core/scene.h"

#include <QHash>
#include <QString>
#include <QVector>

#include <array>

// Turns a scene into the rows the timeline draws. No widget code, so it can
// be tested on its own.
//
//   Background                     layer (expanded by default)
//     ▸ star.png ×200   line 12    group: same layer + line + file + name
//       bg.jpg          line 3     sprite
//         Fade                     command lane (one per command type)
//
// Sprites created by the same script line (a loop, a helper function) are
// folded into one group. Giving them different `name`s keeps them apart.

enum class timeline_row_kind { layer, group, sprite, command_lane };

struct timeline_bar {
    int t0 = 0;
    int t1 = 0;                 // t1 <= t0: an instant (or "from t0 on")

    qsizetype sprite = -1;      // scene index; -1 for merged spans
    qsizetype command = -1;     // index in sprite.cmds; -1 for lifetimes
    cmd_t type = cmd_t::fade;   // valid when command >= 0

    bool is_command() const { return command >= 0; }
};

struct timeline_row {
    timeline_row_kind kind = timeline_row_kind::layer;
    layer_t layer = layer_t::foreground;
    int depth = 0;              // indentation level

    QString label;
    QString detail;             // e.g. "line 12" or "4 sprites"
    int source_line = 0;        // 0 when unknown
    cmd_t command_type = cmd_t::fade;   // command lanes only

    QString key;                // stable across reruns (expand state)
    bool expandable = false;
    bool expanded = false;

    qsizetype first_sprite = -1;    // for selection / tooltips
    qsizetype sprite_count = 0;

    QVector<timeline_bar> bars;
};

class timeline_model {
    public:
        // Layers top to bottom, in osu!'s draw order (back to front).
        static constexpr std::array<layer_t, 5> layer_order{
            layer_t::background, layer_t::fail, layer_t::pass,
            layer_t::foreground, layer_t::overlay
        };

        static QString layer_name(layer_t layer);
        static QString command_name(cmd_t type);

        void set_scene(const scene& sc);
        void clear();

        const scene& current_scene() const { return m_scene; }
        const QVector<timeline_row>& rows() const { return m_rows; }
        bool is_empty() const { return m_scene.sprites.isEmpty(); }

        // Time covered by all commands; begin > end when there are none.
        int time_begin() const { return m_time_begin; }
        int time_end() const { return m_time_end; }

        // Kept per key, so it survives set_scene() with a re-run script.
        void toggle_expanded(int row);
        void set_expanded(const QString& key, bool expanded);

    private:
        void rebuild();
        bool is_expanded(const QString& key, bool fallback) const;

        void add_sprite_rows(qsizetype index, int depth, const QString& key);

        scene m_scene;
        QVector<timeline_row> m_rows;
        QHash<QString, bool> m_expanded;

        int m_time_begin = 1;
        int m_time_end = 0;
};
