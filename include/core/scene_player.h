#pragma once

#include "core/scene.h"

#include <algorithm>
#include <cmath>

#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QHash>

inline double ease(const QString& type, double t){
    if(type == "In"){
        return t * t;
    }

    if(type == "Out"){
        return 1 - (1 - t) * (1 - t);
    }

    return t; //linear
}

inline QPointF origin_offset(origin_t origin, double w, double h){
    switch(origin){
        //top 
        case origin_t::top_left:
            return QPointF(0.0, 0.0);

        case origin_t::top_center:
            return QPointF(w / 2.0, 0.0);
            
        case origin_t::top_right:
            return QPointF(w, 0.0);


        //center
        case origin_t::center_left:
            return QPointF(0.0, h / 2.0);

        case origin_t::center:
            return QPointF(w / 2.0, h / 2.0);

        case origin_t::center_right:
            return QPointF(w, h / 2.0);

        //bottom
        case origin_t::bottom_left:
            return QPointF(0.0, h);

        case origin_t::bottom_center:
            return QPointF(w / 2.0, h);

        case origin_t::bottom_right:
            return QPointF(w, h);
    }

    return QPointF(w / 2.0, h / 2.0);

}

struct sprite_state{
    // Whether osu! shows the sprite at all: only from its first command's
    // start to its last command's end. A sprite without commands never shows.
    bool visible;

    double x;
    double y;
    double alpha;
    double rot;     //rotation, radians

    //scale
    double scale;
    double scale_x;
    double scale_y;

    //flags
    bool flip_h;
    bool flip_v;
    bool additive;

    //colors
    double r;
    double g;
    double b;
};

// Time span in which the sprite exists (osu!: earliest start to latest end).
// Parameter commands (flip, additive) with end <= start only count at start.
struct sprite_lifetime{
    bool valid = false;     // false when there are no commands
    int start = 0;
    int end = 0;
};

inline sprite_lifetime lifetime_of(const sprite& s){
    sprite_lifetime life;

    for(const auto& cmd : s.cmds){
        const int cmd_end = std::max(cmd.t0, cmd.t1);

        if(!life.valid){
            life = {true, cmd.t0, cmd_end};
        }
        else{
            life.start = std::min(life.start, cmd.t0);
            life.end = std::max(life.end, cmd_end);
        }
    }

    return life;
}

/**
 * The sprite's state at ms, following osu!'s rules:
 *  - each property is driven by its own commands (move and moveX both
 *    drive x); the one that started last (at or before ms) wins, and holds
 *    its end value after it ends
 *  - before a property's first command, it already has that command's
 *    start value
 *  - properties without commands keep their defaults (the sprite's
 *    position, opacity 1, scale 1, no rotation, white)
 *  - flip/additive are on from start until end, or for good when end <= start
 */
inline sprite_state eval_sprite(const sprite& s, int ms){
    sprite_state state{
        .visible = false,
        .x = s.x,
        .y = s.y,
        .alpha = 1.0,
        .rot = 0.0,
        .scale = 1.0,
        .scale_x = 1.0,
        .scale_y = 1.0,
        .flip_h = false,
        .flip_v = false,
        .additive = false,
        .r = 255.0,
        .g = 255.0,
        .b = 255.0
    };

    const sprite_lifetime life = lifetime_of(s);
    state.visible = life.valid && ms >= life.start && ms <= life.end;

    // Per property: the command in charge at ms (latest start <= ms; on a
    // tie the one written later), or else the earliest one.
    enum channel { ch_alpha, ch_x, ch_y, ch_scale, ch_scale_vec, ch_rot, ch_color, channel_count };
    const command* active[channel_count] = {};
    const command* first[channel_count] = {};

    auto consider = [ms, &active, &first](channel ch, const command& cmd){
        if(!first[ch] || cmd.t0 < first[ch]->t0){
            first[ch] = &cmd;
        }

        if(cmd.t0 <= ms && (!active[ch] || cmd.t0 >= active[ch]->t0)){
            active[ch] = &cmd;
        }
    };

    auto flag_on = [ms](const command& cmd){
        return ms >= cmd.t0 && (cmd.t1 <= cmd.t0 || ms < cmd.t1);
    };

    for(const auto& cmd : s.cmds){
        switch(cmd.type){
            case cmd_t::fade:       consider(ch_alpha, cmd); break;
            case cmd_t::move:       consider(ch_x, cmd); consider(ch_y, cmd); break;
            case cmd_t::move_x:     consider(ch_x, cmd); break;
            case cmd_t::move_y:     consider(ch_y, cmd); break;
            case cmd_t::scale:      consider(ch_scale, cmd); break;
            case cmd_t::scale_vec:  consider(ch_scale_vec, cmd); break;
            case cmd_t::rotate:     consider(ch_rot, cmd); break;
            case cmd_t::color:      consider(ch_color, cmd); break;

            case cmd_t::flip_h:     state.flip_h = state.flip_h || flag_on(cmd); break;
            case cmd_t::flip_v:     state.flip_v = state.flip_v || flag_on(cmd); break;
            case cmd_t::additive:   state.additive = state.additive || flag_on(cmd); break;
        }
    }

    // Eased progress of a command at ms (0 before it starts, 1 after it ends).
    auto progress = [ms](const command& cmd){
        if(ms < cmd.t0){
            return 0.0;
        }

        const double t = (cmd.t1 <= cmd.t0)
            ? 1.0
            : std::clamp((ms - cmd.t0) / double(cmd.t1 - cmd.t0), 0.0, 1.0);

        return ease(cmd.easing, t);
    };

    auto lerp = [](double a, double b, double t){ return a + (b - a) * t; };

    for(int ch = 0; ch < channel_count; ++ch){
        const command* cmd = active[ch] ? active[ch] : first[ch];

        if(!cmd){
            continue;
        }

        const double t = progress(*cmd);

        switch(ch){
            case ch_alpha:
                state.alpha = lerp(cmd->v0, cmd->v1, t);
                break;

            case ch_x:
                state.x = (cmd->type == cmd_t::move) ? lerp(cmd->x0, cmd->x1, t) : lerp(cmd->v0, cmd->v1, t);
                break;

            case ch_y:
                state.y = (cmd->type == cmd_t::move) ? lerp(cmd->y0, cmd->y1, t) : lerp(cmd->v0, cmd->v1, t);
                break;

            case ch_scale:
                state.scale = lerp(cmd->v0, cmd->v1, t);
                break;

            case ch_scale_vec:
                state.scale_x = lerp(cmd->x0, cmd->x1, t);
                state.scale_y = lerp(cmd->y0, cmd->y1, t);
                break;

            case ch_rot:
                state.rot = lerp(cmd->v0, cmd->v1, t);
                break;

            case ch_color:
                state.r = lerp(cmd->r0, cmd->r1, t);
                state.g = lerp(cmd->g0, cmd->g1, t);
                state.b = lerp(cmd->b0, cmd->b1, t);
                break;
        }
    }

    return state;
}

inline unsigned layer_bit(layer_t layer){
    return 1u << unsigned(layer);
}

constexpr unsigned all_layers_mask = 0x1f;

// The image multiplied by the tint, as osu! colours sprites.
inline QImage tinted(const QPixmap& pix, const QColor& tint){
    QImage image = pix.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Multiply);
    painter.fillRect(image.rect(), tint);

    // Multiply also colours transparent pixels; put the alpha back.
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.drawPixmap(0, 0, pix);

    return image;
}

// Draws in storyboard coordinates (640x480 playfield); the caller maps them
// onto the widget.
inline void draw_sprite(QPainter& p, const sprite& s, int ms, const QHash<QString, QPixmap>& pixmaps){
    const auto state = eval_sprite(s, ms);

    if(!state.visible || state.alpha <= 0.0){
        return;
    }

    p.save();
    p.setOpacity(std::min(state.alpha, 1.0));
    p.setCompositionMode(state.additive ? QPainter::CompositionMode_Plus
                                        : QPainter::CompositionMode_SourceOver);

    const QPixmap* pix = nullptr;
    double width = 32.0;
    double height = 32.0;

    auto it = pixmaps.find(s.path);
    if(it != pixmaps.end() && !it->isNull()){
        pix = &it.value();
        width = pix->width();
        height = pix->height();
    }

    const QPointF offset = origin_offset(s.origin, width, height);
    const QRectF rect(state.x - offset.x(), state.y - offset.y(), width, height);

    // Scale and rotation happen around the origin point.
    const QPointF pivot(state.x, state.y);
    p.translate(pivot);
    p.rotate(state.rot * 180.0 / M_PI);
    p.scale(state.scale * state.scale_x, state.scale * state.scale_y);
    p.translate(-pivot);

    // Flips mirror the image in place, whatever the origin.
    if(state.flip_h || state.flip_v){
        p.translate(rect.center());
        p.scale(state.flip_h ? -1.0 : 1.0, state.flip_v ? -1.0 : 1.0);
        p.translate(-rect.center());
    }

    const QColor tint(
        std::clamp(int(std::lround(state.r)), 0, 255),
        std::clamp(int(std::lround(state.g)), 0, 255),
        std::clamp(int(std::lround(state.b)), 0, 255)
        );

    if(pix){
        if(tint == QColor(Qt::white)){
            p.drawPixmap(rect.topLeft(), *pix);
        }
        else{
            p.drawImage(rect.topLeft(), tinted(*pix, tint));
        }
    }
    else{
        // Placeholder for a missing image.
        p.setPen(QPen(tint, 0));
        p.drawRect(rect);
    }

    p.restore();
}

// Back to front, as osu! draws them; within a layer, in creation order.
inline void draw_scene(QPainter& p, const scene& sc, int ms, const QHash<QString, QPixmap>& pixmaps,
        unsigned visible_layers = all_layers_mask){
    static constexpr layer_t draw_order[] = {
        layer_t::background, layer_t::fail, layer_t::pass, layer_t::foreground, layer_t::overlay
    };

    for(layer_t layer : draw_order){
        if(!(visible_layers & layer_bit(layer))){
            continue;
        }

        for(const auto& s : sc.sprites){
            if(s.layer == layer){
                draw_sprite(p, s, ms, pixmaps);
            }
        }
    }
}
