#include "scripting/js_bridge.h"

js_bridge::js_bridge(QJSEngine *eng, scene *sc, QObject *parent)
    : QObject(parent), eng_(eng), sc_(sc) {}

void js_bridge::begin_run() {
    *sc_ = scene{};
    ++generation_;
}

::sprite *js_bridge::sprite_for(qsizetype index, quint64 generation) {
    if (generation != generation_ || index < 0 || index >= sc_->sprites.size()) {
        return nullptr;
    }

    return &sc_->sprites[index];
}

QObject *js_bridge::add_sprite(const QString &path, const QString &layer,
                               const QString &origin, double x, double y, int source_line) {
    layer_t ly = to_layer(layer);
    origin_t orr = to_origin(origin);

    const qsizetype index = sc_->add_sprite(path, ly, orr, x, y);
    sc_->sprites[index].source_line = source_line;

    return new js_sprite_proxy(this, index, generation_);
}

layer_t js_bridge::to_layer(const QString &s)
{
    if (s.compare("Background", Qt::CaseInsensitive) == 0) {
        return layer_t::background;
    }

    if (s.compare("Foreground", Qt::CaseInsensitive) == 0) {
        return layer_t::foreground;
    }

    if (s.compare("Pass", Qt::CaseInsensitive) == 0) {
        return layer_t::pass;
    }

    if (s.compare("Fail", Qt::CaseInsensitive) == 0) {
        return layer_t::fail;
    }

    if (s.compare("Overlay", Qt::CaseInsensitive) == 0) {
        return layer_t::overlay;
    }

    return layer_t::foreground;
}

origin_t js_bridge::to_origin(const QString &s) {
    if (s.compare("TopLeft", Qt::CaseInsensitive) == 0) {
        return origin_t::top_left;
    }
    if (s.compare("TopCenter", Qt::CaseInsensitive) == 0) {
        return origin_t::top_center;
    }
    if (s.compare("TopRight", Qt::CaseInsensitive) == 0) {
        return origin_t::top_right;
    }

    if (s.compare("CenterLeft", Qt::CaseInsensitive) == 0) {
        return origin_t::center_left;
    }
    if (s.compare("Center", Qt::CaseInsensitive) == 0 ||
            s.compare("Middle", Qt::CaseInsensitive) == 0 ||
            s.compare("CenterCenter", Qt::CaseInsensitive) == 0 ||
            s.compare("Centre", Qt::CaseInsensitive) == 0) {
        return origin_t::center;
    }
    if (s.compare("CenterRight", Qt::CaseInsensitive) == 0) {
        return origin_t::center_right;
    }

    if (s.compare("BottomLeft", Qt::CaseInsensitive) == 0) {
        return origin_t::bottom_left;
    }
    if (s.compare("BottomCenter", Qt::CaseInsensitive) == 0) {
        return origin_t::bottom_center;
    }
    if (s.compare("BottomRight", Qt::CaseInsensitive) == 0) {
        return origin_t::bottom_right;
    }

    return origin_t::center;
}

QObject *js_bridge::sprite(const QString &path, const QJSValue &opts, int source_line) {
    QString layer = QStringLiteral("Foreground");
    QString origin = QStringLiteral("Center");

    double x = 320.0;
    double y = 240.0;

    // parses layer(default: foreground)
    if (opts.hasProperty("layer")) {
        layer = opts.property("layer").toString();
    }

    // parses origin(default: center)
    if (opts.hasProperty("origin")) {
        origin = opts.property("origin").toString();
    }

    // parses position(default: [320, 240])
    if (opts.hasProperty("position")) {
        QJSValue posArr = opts.property("position");

        if (posArr.isArray()) {
            QJSValue px = posArr.property(0);
            QJSValue py = posArr.property(1);

            if(px.isNumber()){
                x = px.toNumber();
            }
            if(py.isNumber()){
                y = py.toNumber();
            }
        }
    }

    if(opts.hasProperty("x")){
        QJSValue px = opts.property("x");
        if(px.isNumber()){
            x = px.toNumber();
        }
    }

    if(opts.hasProperty("y")){
        QJSValue py = opts.property("y");
        if(py.isNumber()){
            y = py.toNumber();
        }
    }

    QObject *proxy = add_sprite(path, layer, origin, x, y, source_line);

    if (opts.hasProperty("name")) {
        sc_->sprites.back().name = opts.property("name").toString();
    }

    return proxy;
}
