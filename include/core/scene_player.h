#pragma once

#include "core/scene.h"
#include "core/sprite_timing.h"

#include <algorithm>
#include <cmath>

#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QHash>

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
 *  - loop commands count once per pass, each pass shifted by its duration
 *  - trigger commands are ignored (they need gameplay)
 *  - opacity above 1 wraps around (1.25 shows as 0.25), as in osu!
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

    const time_span life = lifetime_of(s);
    state.visible = life.valid && ms >= life.start && ms <= life.end;

    // A command at its place in time: loop commands are shifted to the
    // start of a pass.
    struct placed{
        const command* cmd = nullptr;
        int offset = 0;

        int t0() const { return cmd->t0 + offset; }
        int t1() const { return cmd->t1 + offset; }
    };

    // Per property: the command in charge at ms (latest start <= ms; on a
    // tie the one written later), or else the earliest one.
    enum channel { ch_alpha, ch_x, ch_y, ch_scale, ch_scale_vec, ch_rot, ch_color, channel_count };
    placed active[channel_count] = {};
    placed first[channel_count] = {};

    auto consider = [ms, &active, &first](channel ch, placed earliest, placed latest){
        if(!first[ch].cmd || earliest.t0() < first[ch].t0()){
            first[ch] = earliest;
        }

        if(latest.t0() <= ms && (!active[ch].cmd || latest.t0() >= active[ch].t0())){
            active[ch] = latest;
        }
    };

    auto flag_on = [ms](placed c){
        return ms >= c.t0() && (c.cmd->t1 <= c.cmd->t0 || ms < c.t1());
    };

    // earliest: its first occurrence; latest: the last one started by ms
    // (or the first, when none has started yet).
    auto add = [&](placed earliest, placed latest){
        switch(earliest.cmd->type){
            case cmd_t::fade:       consider(ch_alpha, earliest, latest); break;
            case cmd_t::move:       consider(ch_x, earliest, latest); consider(ch_y, earliest, latest); break;
            case cmd_t::move_x:     consider(ch_x, earliest, latest); break;
            case cmd_t::move_y:     consider(ch_y, earliest, latest); break;
            case cmd_t::scale:      consider(ch_scale, earliest, latest); break;
            case cmd_t::scale_vec:  consider(ch_scale_vec, earliest, latest); break;
            case cmd_t::rotate:     consider(ch_rot, earliest, latest); break;
            case cmd_t::color:      consider(ch_color, earliest, latest); break;

            case cmd_t::flip_h:     state.flip_h = state.flip_h || flag_on(latest); break;
            case cmd_t::flip_v:     state.flip_v = state.flip_v || flag_on(latest); break;
            case cmd_t::additive:   state.additive = state.additive || flag_on(latest); break;
        }
    };

    for(const auto& cmd : s.cmds){
        add({&cmd, 0}, {&cmd, 0});
    }

    for(const auto& loop : s.loops){
        const int pass_length = timing_of(loop).duration();
        const int last_pass = loop_passes(loop) - 1;

        for(const auto& cmd : loop.cmds){
            const placed earliest{&cmd, loop.start};
            int pass = 0;

            if(ms >= earliest.t0()){
                pass = pass_length > 0
                    ? int(std::min<qint64>(last_pass, (qint64(ms) - earliest.t0()) / pass_length))
                    : last_pass;
            }

            add(earliest, {&cmd, loop.start + pass * pass_length});
        }
    }

    // Eased progress of a command at ms (0 before it starts, 1 after it ends).
    auto progress = [ms](placed c){
        if(ms < c.t0()){
            return 0.0;
        }

        const double t = (c.t1() <= c.t0())
            ? 1.0
            : std::clamp((ms - c.t0()) / double(c.t1() - c.t0()), 0.0, 1.0);

        return apply_easing(c.cmd->easing, t);
    };

    auto lerp = [](double a, double b, double t){ return a + (b - a) * t; };

    for(int ch = 0; ch < channel_count; ++ch){
        const placed c = active[ch].cmd ? active[ch] : first[ch];

        if(!c.cmd){
            continue;
        }

        const command* cmd = c.cmd;
        const double t = progress(c);

        switch(ch){
            case ch_alpha:
                state.alpha = lerp(cmd->v0, cmd->v1, t);

                // osu! wraps opacity above 1; storyboards use it for flicker.
                if(state.alpha > 1.0){
                    state.alpha = std::fmod(state.alpha, 1.0);
                }
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
    p.setOpacity(state.alpha);
    p.setCompositionMode(state.additive ? QPainter::CompositionMode_Plus
                                        : QPainter::CompositionMode_SourceOver);

    const QPixmap* pix = nullptr;
    double width = 32.0;
    double height = 32.0;

    auto it = pixmaps.find(s.is_animation() ? frame_path(s.path, animation_frame(s, ms)) : s.path);
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

    // Where the origin lands on screen (flips below don't move it).
    const QPointF screen_pivot = p.transform().map(pivot);

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
        // Missing image: magenta, never tinted, so it can't pass for a
        // coloured sprite. The marker at the origin stays a few pixels on
        // screen however small the sprite is scaled.
        const QColor missing(255, 0, 255);
        p.setPen(QPen(missing, 0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect);
        p.drawLine(rect.topLeft(), rect.bottomRight());
        p.drawLine(rect.topRight(), rect.bottomLeft());

        p.resetTransform();
        p.drawLine(screen_pivot - QPointF(4, 4), screen_pivot + QPointF(4, 4));
        p.drawLine(screen_pivot - QPointF(-4, 4), screen_pivot + QPointF(-4, 4));
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
