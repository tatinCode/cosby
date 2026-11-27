#pragma once

#include <QString> 
#include <QVector>
#include <QHash>
#include <QPixmap>

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

    QString easing{"Linear"};

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
};

struct scene
{
    QVector<sprite> sprites;

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
