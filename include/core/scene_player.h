#pragma once

#include "core/scene.h"
#include <QPainter>
#include <algorithm>
#include <cmath>

inline double ease(const QString& type, double t){
    if(type == "In"){
        return t * t;
    }

    if(type == "Out"){
        return 1 - (1 - t) * (1 - t);
    }

    return t; //linear
}

struct sprite_state{
    double x;
    double y;
    double alpha;
    double rot;     //rotation

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

inline sprite_state eval_sprite(const sprite& s, int ms){
    sprite_state state{
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

    for(const auto& cmd : s.cmds){
        if(ms < cmd.t0){
            continue;
        }

        double normalized_t = (cmd.t1 == cmd.t0) ? 
            1.0 : std::clamp((ms - cmd.t0) / double(cmd.t1 - cmd.t0), 0.0, 1.0);

        double eased_t = ease(cmd.easing, normalized_t);

        switch(cmd.type){
            case cmd_t::fade:
                state.alpha =  cmd.v0 + (cmd.v1 - cmd.v0) * eased_t;
                break;

            case cmd_t::move:
                state.x = cmd.x0 + (cmd.x1 - cmd.x0) * eased_t;
                state.y = cmd.y0 + (cmd.y1 - cmd.y0) * eased_t;
                break;

            case cmd_t::move_x:
                state.x = cmd.v0 + (cmd.v1 - cmd.v0) * eased_t;
                break;

            case cmd_t::move_y:
                state.y = cmd.v0 + (cmd.v1 - cmd.v0) * eased_t;
                break;

            case cmd_t::scale:
                state.scale = cmd.v0 + (cmd.v1 - cmd.v0) * eased_t;
                break;

            case cmd_t::scale_vec:
                state.scale_x = cmd.x0 + (cmd.x1 - cmd.x0) * eased_t;
                state.scale_y = cmd.y0 + (cmd.y1 - cmd.y0) * eased_t;
                break;

            case cmd_t::rotate:
                state.rot = cmd.v0 + (cmd.v1 - cmd.v0) * eased_t;
                break;

            //flip could either be timed or persistent throughout.
            case cmd_t::flip_h:
                state.flip_h = (ms >= cmd.t0) && (cmd.t1 <= cmd.t0 || ms <= cmd.t1);
                break;

            case cmd_t::flip_v:
                state.flip_v = (ms >= cmd.t0) && (cmd.t1 <= cmd.t0 || ms <= cmd.t1);
                break;

            case cmd_t::additive:
                state.additive = (ms >= cmd.t0) && (cmd.t1 <= cmd.t0 || ms <= cmd.t1);
                break;

            case cmd_t::color:
                state.r = cmd.r0 + (cmd.r1 - cmd.r0) * eased_t;
                state.g = cmd.g0 + (cmd.g1 - cmd.g0) * eased_t;
                state.b = cmd.b0 + (cmd.b1 - cmd.b0) * eased_t;
                break;
                
            default:
                break;
        }
    }

    return state;
}

inline void draw_scene(QPainter& p, const scene& sc, int ms){
    for(const auto& s : sc.sprites){
        auto state = eval_sprite(s, ms);

        p.save();
        p.setOpacity(std::clamp(state.alpha, 0.0, 1.0));

        if(state.additive){
            p.setCompositionMode(QPainter::CompositionMode_Plus);
        } else{
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }

        QRectF rect(state.x - 16, state.y - 16, 32, 32);

        const double flip_scale_x = state.flip_h ? -1.0 : 1.0;
        const double flip_scale_y = state.flip_v ? -1.0 : 1.0;
        const double final_scale_x = state.scale * state.scale_x * flip_scale_x;
        const double final_scale_y = state.scale * state.scale_y * flip_scale_y;

        p.translate(rect.center());
        p.rotate(state.rot * 180.0 / M_PI);
        p.scale(final_scale_x, final_scale_y);
        p.translate(-rect.center());

        QColor tint(
            std::clamp(int(std::lround(state.r)), 0, 255),
            std::clamp(int(std::lround(state.g)), 0, 255),
            std::clamp(int(std::lround(state.b)), 0, 255)
            );

        p.setPen(tint);
        p.drawRect(rect);

        p.restore();
    }
}


