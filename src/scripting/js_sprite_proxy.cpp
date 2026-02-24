#include <QJSValue>
#include <QVariant>
#include <QDebug>

#include "scripting/js_sprite_proxy.h"

js_sprite_proxy *js_sprite_proxy::fade(const QString &easing, int t0, int t1,
                                       double a0, double a1)
{

    s_->cmds.push_back({cmd_t::fade, easing, t0, t1, a0, a1});
    return this;
}

js_sprite_proxy *js_sprite_proxy::move(const QString &easing, int t0, int t1,
                                       double x0, double y0, double x1, double y1)
{
    command cmd;
    cmd.type = cmd_t::move;
    cmd.easing = easing;
    cmd.t0 = t0;
    cmd.t1 = t1;
    cmd.x0 = x0;
    cmd.y0 = y0;
    cmd.x1 = x1;
    cmd.y1 = y1;

    s_->cmds.push_back(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::scale(const QString &easing, int t0, int t1,
                                        double s0, double s1)
{
    command cmd;

    cmd.type = cmd_t::scale;

    cmd.easing = easing;
    cmd.t0 = t0;
    cmd.t1 = t1;

    cmd.s0 = s0;
    cmd.s1 = s1;

    s_->cmds.push_back(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::rotate(const QString &easing, int t0, int t1,
                                         double r0, double r1)
{
    command cmd;

    cmd.type = cmd_t::rotate;

    cmd.easing = easing;
    cmd.t0 = t0;
    cmd.t1 = t1;

    cmd.r0 = r0;
    cmd.r1 = r1;

    s_->cmds.push_back(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::color(const QString &easing, int t0, int t1,
                                        double r, double g, double b)
{
    command cmd;

    cmd.type = cmd_t::color;

    cmd.easing = easing;
    cmd.t0 = t0;
    cmd.t1 = t1;

    cmd.start_params = {r, g, b};
    cmd.end_params = {r, g, b};

    s_->cmds.push_back(cmd);

    return this;
}

static QString readEasing(const QJSValue &opts)
{
    if (opts.hasProperty("easing"))
    {
        return opts.property("easing").toString();
    }

    return QStringLiteral("Linear");
}

static QString readInt(const QJSValue &obj, const char *key, int def = 0)
{
    return obj.hasProperty(key) ? opts.property(key).toInt() : def;
}

static QString readNumber(const QJSValue &obj, const char *key, double def = 0.0)
{
    return obj.hasProperty(key) ? obj.property(key).toNumber() : def;
}

static bool readArray2(const QJSValue &obj, const char *key, double &out0, double &out1)
{
    if (!obj.hasProperty(key))
    {
        return false;
    }

    QJSValue arr = obj.property(key);

    if (!arr.isArray())
    {
        return false;
    }

    QJSValue x = arr.property(0);
    QJSValue y = arr.property(1);

    if (!x.isNumber() || !y.isNumber())
    {
        return false;
    }

    out0 = x.toNumber();
    out1 = y.toNumber();

    return true;
}

// object warpers:

js_sprite_proxy *js_sprite_proxy::fade(const QJSValue opts)
{
    const QString easing = readEasing(opts);
    const int t0 = readInt(opts, "startTime", 0);
    const int t1 = readInt(opts, "endTime", 0);

    const double a0 = readNumber(opts, "startValue", 0.0);
    const double a1 = readNumber(opts, "endValue", 0.0);

    return fade(easing, t0, t1, a0, a1);
}

js_sprite_proxy *js_sprite_proxy::move(const QJSValue opts)
{
    const QString easing = readEasing(opts);

    const int t0 = readInt(opts, "startTime", 0);
    const int t1 = readInt(opts, "endTime", 0);

    double x0{};
    double y0{};

    double x1{};
    double y1{};

    if (!readArray2(opts, "startPosition", x0, y0))
    {
        x0 = readNumber(opts, "startX", readNumber(opts, "x", 0.0));
        y0 = readNumber(opts, "startY", readNumber(opts, "y", 0.0));
    }

    if (!readArray2(opts, "endPosition", x1, y1))
    {
        x1 = readNumber(opts, "endX", x0);
        y1 = readNumber(opts, "endY", y0);
    }

    return move(easing, t0, t1, x0, y0, x1, y1);
}

js_sprite_proxy *js_sprite_proxy::scale(const QJSValue opts)
{
    const QString easing = readEasing(opts);

    const int t0 = readInt(opts, "startTime", 0);
    const int t1 = readInt(opts, "endTime", 0);

    const double s0 = readNumber(opts, "startValue", 1.0);
    const double s1 = readNumber(opts, "endValue", 1.0);

    return scale(easing, t0, t1, s0, s1);
}

js_sprite_proxy *js_sprite_proxy::rotate(const QJSValue opts)
{
    const QString easing = readEasing(opts);

    const int t0 = readInt(opts, "startTime", 0);
    const int t1 = readInt(opts, "endTime", 0);

    const double r0 = readNumber(opts, "startValue", 0.0);
    const double r1 = readNumber(opts, "endValue", 0.0);

    return rotate(easing, t0, t1, r0, r1);
}

// object wrapper for color does not support animated color,
// but will be added later
js_sprite_proxy *js_sprite_proxy::color(const QJSValue opts)
{
    const QString easing = readEasing(opts);

    const int t0 = readInt(opts, "startTime", 0);
    const int t1 = readInt(opts, "endTime", 0);

    double r = 255, g = 255, b = 255;
    double r0 = 255, g0 = 255, b0 = 255;

    if (readArray2(opts, "endValue", r, g))
    {
        // endValue is [r, g, b] - we already read r, g; try b:
        QJSValue arr = opts.property("endValue");
        QJSValue bz = arr.property(2);

        if (bz.isNumber())
        {
            b = bz.toNumber();
        }
    }
    else
    {
        r = readNumber(opts, "r", r);
        g = readNumber(opts, "g", g);
        b = readNumber(opts, "b", b);
    }

    // we ignore startValue until coloranimation is supported
    if (readArray2(opts, "startValue", r0, g0))
    {
        QJSValue arr = opts.property("startValue");
        QJSValue bz = arr.property(2);
        if (bz.isNumber())
        {
            b0 = bz.toNumber();
        }
    }

    return color(easing, t0, t1, r, g, b);
}

js_sprite_proxy *js_sprite_proxy::moveX(const QJSValue &opts)
{
    command cmd;

    cmd.type = cmd_t::move_x;
    cmd.easing = readEasing(opts);
    cmd.start_time = readInt(opts, "startTime", 0);
    cmd.end_time = readInt(opts, "endTime", 0);
    cmd.start_params = {readNumber(opts, "startValue", s_->x)};
    cmd.end_params = {readNumber(opts, "endValue", s_->x)};

    s_->cmds.push_back(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::moveY(const QJSValue &opts)
{
    command cmd;

    cmd.type = cmd_t::move_y;
    cmd.easing = readEasing(opts);
    cmd.start_time = readInt(opts, "startTime", 0);
    cmd.end_time = readInt(opts, "endTime", 0);
    cmd.start_params = {readNumber(opts, "startValue", s_->y)};
    cmd.end_params = {readNumber(opts, "endValue", s_->y)};

    s_->cmds.push_back(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::scaleVec(const QJSValue &opts)
{
    command cmd;

    cmd.type = cmd_t::scale_vec;

    cmd.easing = readEasing(opts);
    cmd.start_time = readInt(opts, "startTime", 0);
    cmd.end_time = readInt(opts, "endTime", 0);

    double sx0 = 1.0, sy0 = 1.0;
    double sx1 = 1.0, sy1 = 1.0;

    readArray2(opts, "startValue", sx0, sy0);
    readArray2(opts, "endValue", sx1, sy1);

    cmd.start_params = {sx0, sy0};
    cmd.end_params = {sx1, sy1};
    s_->cmds.push_back(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::flipH(const QJSValue &opts)
{
    command cmd;

    cmd.type = cmd_t::flip_h;
    cmd.start_time = readInt(opts, "startTime", 0);
    cmd.end_time = readInt(opts, "endTime", 0);

    s_->cmds.push_back(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::flipV(const QJSValue &opts)
{
    command cmd;

    cmd.type = cmd_t::flip_v;
    cmd.start_time = readInt(opts, "startTime", 0);
    cmd.end_time = readInt(opts, "endTime", 0);

    s_->cmds.push_back(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::additive(const QJSValue &opts)
{
    command cmd;

    cmd.type = cmd_t::additive;
    cmd.start_time = readInt(opts, "startTime", 0);
    cmd.end_time = readInt(opts, "endTime", 0);

    s_->cmds.push_back(cmd);

    return this;
}
