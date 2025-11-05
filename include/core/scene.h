#pragma once

#include <QString>
#include <QVector>

enum class layer_t {
    background,
    foreground,
    pass,
    fail
};

enum class origin_t {
    top_left,
    bottom_left,
    center,
};

enum class cmd_t{
    fade,
    move,
    scale,
    rotate,
    color
};
struct command{
    cmd_t type{};

    QString easing{"Linear"};

    int t0{0}, t1{0};
    double a0{}, a1{};      //fade
    double r0{}, r1{};      //rotate 
    double s0{}, s1{};      //scale
    double x0{}, y0{}, 
           x1{}, y1{};      //move
    double r{}, g{}, b{};   //color
};

struct sprite{
    QString path;

    layer_t layer {layer_t::foreground};
    origin_t origin {origin_t::center};
    double x{0}, y{0};

    QVector<command> cmds;
};

struct scene{
    QVector<sprite> sprites;

    sprite& add_sprite(QString path, layer_t layer, origin_t origin, double x, double y){
        sprites.push_back(sprite{std::move(path), layer, origin, x, y, {}});
        return sprites.back();
    }
};
