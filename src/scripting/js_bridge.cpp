#include "scripting/js_bridge.h"

#include <cmath>

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

QObject *js_bridge::animation(const QString &path, const QJSValue &opts, int source_line) {
    const QJSValue count = opts.property("frameCount");
    const QJSValue delay = opts.property("frameDelay");

    if (!count.isNumber() || count.toNumber() < 1 || count.toNumber() != std::floor(count.toNumber())) {
        eng_->throwError(QJSValue::RangeError,
                         QStringLiteral("animation(path, options): options.frameCount must be a whole number of frames, at least 1"));
        return nullptr;
    }

    if (!delay.isNumber() || !(delay.toNumber() > 0)) {
        eng_->throwError(QJSValue::RangeError,
                         QStringLiteral("animation(path, options): options.frameDelay must be the milliseconds per frame, above 0"));
        return nullptr;
    }

    animation_loop_t loop_type = animation_loop_t::loop_forever;

    if (opts.hasProperty("loopType")) {
        const QString text = opts.property("loopType").toString();

        if (text.compare("LoopOnce", Qt::CaseInsensitive) == 0) {
            loop_type = animation_loop_t::loop_once;
        }
        else if (text.compare("LoopForever", Qt::CaseInsensitive) != 0) {
            eng_->throwError(QJSValue::TypeError,
                             QStringLiteral("Unknown loopType \"%1\". Use \"LoopForever\" or \"LoopOnce\"").arg(text));
            return nullptr;
        }
    }

    QObject *proxy = sprite(path, opts, source_line);

    ::sprite &s = sc_->sprites.back();
    s.frame_count = count.toInt();
    s.frame_delay = delay.toNumber();
    s.loop_type = loop_type;

    return proxy;
}

void js_bridge::sample(int time, const QString &path, const QJSValue &opts, int source_line) {
    ::sample smp;
    smp.time = time;
    smp.path = path;
    smp.source_line = source_line;

    if (opts.hasProperty("layer")) {
        const QString layer = opts.property("layer").toString();

        if (layer.compare("Overlay", Qt::CaseInsensitive) == 0) {
            eng_->throwError(QJSValue::RangeError,
                             QStringLiteral("Samples can't use the Overlay layer; use Background, Fail, Pass or Foreground"));
            return;
        }

        smp.layer = to_layer(layer);
    }

    if (opts.hasProperty("volume")) {
        const QJSValue volume = opts.property("volume");

        if (!volume.isNumber() || volume.toNumber() < 0 || volume.toNumber() > 100) {
            eng_->throwError(QJSValue::RangeError,
                             QStringLiteral("sample(time, path, options): options.volume must be 0-100"));
            return;
        }

        smp.volume = qRound(volume.toNumber());
    }

    sc_->samples.push_back(smp);
}
