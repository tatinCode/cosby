#include "scripting/js_proxy_proxy.h"

js_sprite_proxy* js_sprite_proxy::fade(const QString& easing, int t0, int t1,
        double a0, double a1){

    s_->cmds.push_back({cmd_t::fade, easing, t0, t1, a0, a1});
    return this;
}

js_sprite_proxy* js_sprite_proxy::move(const QString& easing, int t0, int t1,
        double x0, double y0, double x1, double y1){
    command c;
    c.type = cmd_t::move;
    c.easing = easing;
    c.t0 = t0;
    c.t1 = t1;
    
    c.x0 = x0;
    c.y0 = y0;
    c.x1 = x1;
    c.y1 = y1;

    s_->cmds.push_back(c);

    return this;
}

js_sprite_proxy* js_sprite_proxy::scale(const QString& easing, int t0, int t1,
        double s0, double s1){
    command c;

    c.type = cmd_t::scale;

    c.easing = easing;
    c.t0 = t0;
    c.t1 = t1;

    c.s0 = s0;
    c.s1 = s1;

    s_->cmds.push_back(c);

    return this;
}

js_sprite_proxy* js_sprite_proxy::rotate(const QString& easing, int t0, int t1,
        double r0, double r1){
    command c;

    c.type = cmd_t::rotate;

    c.easing = easing;
    c.t0 = t0;
    c.t1 = t1;

    c.r0 = r0;
    c.r1 = r1;

    s_->cmds.push_back(c);

    return this;
}

js_sprite_proxy* js_sprite_proxy::color(const QString& easing, int t0, int t1,
        double r, double g, double b){
    command c;


    c.type = cmd_t::rotate;

    c.easing = easing;
    c.t0 = t0;
    c.t1 = t1;

    c.r = r;
    c.g = r;
    c.r = r;

    s_->cmds.push_back(c);

    return this;
}
