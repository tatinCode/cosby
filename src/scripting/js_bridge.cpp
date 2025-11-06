#include "scripting/js_bridge.h"

js_bridge::js_bridge(QJSEngine* eng, scene* sc, QObject* parent)
    : QObject(parent), eng_(eng), sc_(sc) {}

QObject* js_bridge::add_sprite(const QString& path, const QString& layer,
        const QString& origin, double x, double y){
    layer_t ly = to_layer(layer);
    origin_t orr = to_origin(origin);

    ::sprite& spr = sc_->add_sprite(path, ly, orr, x, y);

    return new js_sprite_proxy(&spr, eng_);
}

QObject* js_bridge::sprite(const QString& path, const QJSValue& opts){
    QString layer = opts.property("layer").toString();
    QString origin = opts.property("origin").toString();

    double x = opts.property("x").toNumber();
    double y = opts.property("y").toNumber();

    return add_sprite( 
            path, 
            layer.isEmpty() ? "Foreground" : layer,
            origin.isEmpty() ? "Centre" : origin,
            x, y
            );
}

layer_t js_bridge::to_layer(const QString& s){
    if(s.compare("Background", Qt::CaseInsensitive) == 0){
        return layer_t::background;
    }

    if(s.compare("Foreground", Qt::CaseInsensitive) == 0){
        return layer_t::foreground;
    }

    if(s.compare("Pass", Qt::CaseInsensitive) == 0){
        return layer_t::pass;
    }

    if(s.compare("Fail", Qt::CaseInsensitive) == 0){
        return layer_t::fail;
    }
}

origin_t js_bridge::to_origin(const QString& s){
    if(s.compare("TopLeft", Qt::CaseInsensitive) == 0){
        return origin_t::top_left;
    }

    if(s.compare("BottomLeft", Qt::CaseInsensitive) == 0){
        return origin_t::bottom_left;
    }

    return origin_t::center;
}

