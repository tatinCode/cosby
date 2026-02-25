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
    double scale;
    double rot;     //rotation
};

inline sprite_state eval_sprite(const sprite& s, int ms){
    sprite_state st(s.x, s.y, 1.0, 1.0, 1.0);

    for(const auto& c : s.cmds){
        if(ms < c.t0){
            continue;
        }

        double u = (c.t1 == c.t0) ? 
            1.0 : std::clamp((ms - c.t0) / double(c.t1 - c.t0), 0.0, 1.0);

        double k = ease(c.easing, u);

        switch(c.type){
            case cmd_t::fade:
                st.alpha =  c.v0 + (c.v1 - c.v0) * k;
                break;

            case cmd_t::move:
                st.x = c.x0 + (c.x1 - c.x0) * k;
                st.y = c.y0 + (c.y1 - c.y0) * k;
                break;

            case cmd_t::scale:
                st.scale = c.s0 + (c.s1 - c.s0) * k;
                break;

            case cmd_t::rotate:
                st.rot = c.r0 + (c.r1 - c.r0) * k;
                break;

            case cmd_t::color:
                /*to do:*/
                break;
                
            default:
                break;
        }
    }

    return st;
}

inline void draw_scene(QPainter& p, const scene& sc, int ms){
    for(const auto& s : sc.sprites){
        auto st = eval_sprite(s, ms);

        p.save();
        p.setOpacity(std::clamp(st.alpha, 0.0, 1.0));
        QRectF rect(st.x - 16, st.y - 16, 32, 32);
        p.translate(rect.center());
        p.rotate(st.rot * 180.0 / M_PI);
        p.scale(st.scale, st.scale);
        p.translate(-rect.center());
        p.setPen(Qt::white);
        p.drawRect(rect);

        p.restore();
    }
}


