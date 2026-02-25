#include "scripting/js_bridge.h"

js_bridge::js_bridge(QJSEngine *eng, scene *sc, QObject *parent)
    : QObject(parent), eng_(eng), sc_(sc) {}

QObject *js_bridge::add_sprite(const QString &path, const QString &layer,
                               const QString &origin, double x, double y)
{
    layer_t ly = to_layer(layer);
    origin_t orr = to_origin(origin);

    ::sprite &spr = sc_->add_sprite(path, ly, orr, x, y);

    return new js_sprite_proxy(&spr, eng_);
}

layer_t js_bridge::to_layer(const QString &s)
{
    if (s.compare("Background", Qt::CaseInsensitive) == 0)
    {
        return layer_t::background;
    }

    if (s.compare("Foreground", Qt::CaseInsensitive) == 0)
    {
        return layer_t::foreground;
    }

    if (s.compare("Pass", Qt::CaseInsensitive) == 0)
    {
        return layer_t::pass;
    }

    if (s.compare("Fail", Qt::CaseInsensitive) == 0)
    {
        return layer_t::fail;
    }

    if (s.compare("Overlay", Qt::CaseInsensitive) == 0)
    {
        return layer_t::overlay;
    }

    return layer_t::foreground;
}

origin_t js_bridge::to_origin(const QString &s)
{
    if (s.compare("TopLeft", Qt::CaseInsensitive) == 0)
    {
        return origin_t::top_left;
    }

    if (s.compare("BottomLeft", Qt::CaseInsensitive) == 0)
    {
        return origin_t::bottom_left;
    }

    return origin_t::center;
}

QObject *js_bridge::sprite(const QString &path, const QJSValue &opts)
{
    ::sprite s;
    s.path = path;

    // parses origin(default: center)
    if (opts.hasProperty("origin"))
    {
        QString originStr = opts.property("origin").toString();

        if (originStr == "TopLeft")
        {
            s.origin = origin_t::top_left;
        }
        else if (originStr == "TopCenter")
        {
            s.origin = origin_t::top_center;
        }
        else if (originStr == "TopRight")
        {
            s.origin = origin_t::top_right;
        }

        else if (originStr == "CenterLeft")
        {
            s.origin = origin_t::center_left;
        }
        else if (originStr == "CenterCenter")
        {
            s.origin = origin_t::center;
        }
        else if (originStr == "CenterRight")
        {
            s.origin = origin_t::center_right;
        }

        else if (originStr == "BottomLeft")
        {
            s.origin = origin_t::bottom_left;
        }
        else if (originStr == "BottomCenter")
        {
            s.origin = origin_t::bottom_center;
        }
        else if (originStr == "BottomRight")
        {
            s.origin = origin_t::bottom_right;
        }

        else
        {
            s.origin = origin_t::center;
        }
    }

    if (opts.hasProperty("layer"))
    {
        QString layerStr = opts.property("layer").toString();

        if (layerStr == "Background")
        {
            s.layer = layer_t::background;
        }
        else if (layerStr == "Foreground")
        {
            s.layer = layer_t::foreground;
        }
        else if (layerStr == "Pass")
        {
            s.layer = layer_t::pass;
        }
        else if (layerStr == "Fail")
        {
            s.layer = layer_t::fail;
        }
        else if (layerStr == "Overlay")
        {
            s.layer = layer_t::overlay;
        }
    }
    else
    {
        s.layer = layer_t::foreground;
    }

    // parse position as x,y (default 0,0)
    if (opts.hasProperty("position"))
    {
        QJSValue posArr = opts.property("position");

        if (posArr.isArray())
        {
            s.x = posArr.property(0).toNumber();
            s.y = posArr.property(1).toNumber();
        }
    }
    else
    {
        s.x = opts.hasProperty("x") ? opts.property("x").toNumber() : 320.0;
        s.y = opts.hasProperty("y") ? opts.property("y").toNumber() : 240.0;
    }

    sc_->sprites.push_back(s);
    auto *proxy = new js_sprite_proxy(&sc_->sprites.back(), this);
    return proxy;
}
