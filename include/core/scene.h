#pragma once

#include <QString> 
#include <QVector>
#include <QHash>
#include <QPixmap>

#include "core/easing.h"

enum class layer_t
{
    background, // 0
    foreground, // 1
    pass,       // 2
    fail,       // 3
    overlay     // 4
};

enum class origin_t
{
    top_left,
    top_center,
    top_right,

    center_left,
    center,
    center_right,

    bottom_left,
    bottom_center,
    bottom_right,
};

enum class cmd_t
{
    fade,
    move,
    scale,
    rotate,
    color,
    move_x,
    move_y,
    scale_vec,
    flip_h,
    flip_v,
    additive
};

struct command
{
    cmd_t type{cmd_t::move};

    easing_t easing{easing_t::linear};

    //time start and end
    int t0{0};
    int t1{0};

    //generic scalar interpolation parameters
    // (fade, scale, rotate, moveX, moveY)
    double v0 {0.0};
    double v1 {0.0};

    //2d interpolation parameters (move, scaleVec)
    double x0{0.0};
    double y0{0.0};
    double x1{0.0};
    double y1{0.0};

    //color interpolation (RGB)
    double r0{255.0};
    double g0{255.0};
    double b0{255.0};
    double r1{255.0};
    double g1{255.0};
    double b1{255.0};
};

// L: commands played `count` times in a row, starting at `start`. The
// commands' times are relative to `start`; one pass lasts from the earliest
// start to the latest end among them.
struct loop_group
{
    int start{0};
    int count{1};           // total passes, at least 1
    QVector<command> cmds;
};

// T: commands played each time `trigger` fires between `start` and `end`
// (e.g. on a clap hitsound). The commands' times are relative to that moment.
// osu! needs gameplay for this, so the preview doesn't play them.
struct trigger_group
{
    QString trigger;        // "Passing", "Failing", "HitSoundClap", ...
    int start{0};
    int end{0};
    int group{0};           // 0: none; triggers in the same group cancel each other
    QVector<command> cmds;
};

enum class animation_loop_t { loop_forever, loop_once };

struct sprite
{
    QString path;

    layer_t layer{layer_t::foreground};
    origin_t origin{origin_t::center};
    double x{0}, y{0};

    QVector<command> cmds;

    // Where the script created it (1-based; 0 = unknown, e.g. preload code).
    int source_line{0};

    // Optional label from sprite(path, { name }); shown in the timeline.
    QString name;

    QVector<loop_group> loops;
    QVector<trigger_group> triggers;

    // An animation when frame_count > 0: frames are path with the frame
    // number before the extension ("flame.png" -> "flame0.png", "flame1.png").
    int frame_count{0};
    double frame_delay{0.0};    // ms per frame
    animation_loop_t loop_type{animation_loop_t::loop_forever};

    bool is_animation() const { return frame_count > 0; }
};

// A sound played once at `time` (osu! "Sample" event).
struct sample
{
    int time{0};
    layer_t layer{layer_t::foreground};     // never overlay; osu! has no such sample layer
    QString path;
    int volume{100};        // 0-100
    int source_line{0};
};

struct scene
{
    QVector<sprite> sprites;
    QVector<sample> samples;

    // Returns the new sprite's index. Not a reference: the next add can
    // reallocate the vector, so anything kept across adds must use indices.
    qsizetype add_sprite(QString path, layer_t layer, origin_t origin, double x, double y)
    {
        sprite s;
        s.path = std::move(path);
        s.layer = layer;
        s.origin = origin;
        s.x = x;
        s.y = y;

        sprites.push_back(std::move(s));
        return sprites.size() - 1;
    }
};
