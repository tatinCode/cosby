#include "scripting/js_bridge.h"
#include "scripting/js_options.h"
#include "core/name_hint.h"

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

namespace {
    // In osu!'s draw order; the names a message suggests.
    const QStringList layer_names{
        QStringLiteral("Background"), QStringLiteral("Fail"), QStringLiteral("Pass"),
        QStringLiteral("Foreground"), QStringLiteral("Overlay")
    };

    const QStringList origin_names{
        QStringLiteral("TopLeft"), QStringLiteral("TopCenter"), QStringLiteral("TopRight"),
        QStringLiteral("CenterLeft"), QStringLiteral("Center"), QStringLiteral("CenterRight"),
        QStringLiteral("BottomLeft"), QStringLiteral("BottomCenter"), QStringLiteral("BottomRight")
    };

    // Also accepted for Center, but not suggested.
    const QStringList center_aliases{
        QStringLiteral("Centre"), QStringLiteral("Middle"), QStringLiteral("CenterCenter")
    };

    const QStringList sprite_keys{
        QStringLiteral("layer"), QStringLiteral("origin"), QStringLiteral("position"),
        QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("name")
    };

    QString unknown_name(const char* what, const QString& name, const QStringList& names){
        return QStringLiteral("Unknown %1 \"%2\".%3 Use one of: %4").arg(
                QString::fromLatin1(what), name, did_you_mean(name, names), names.join(QStringLiteral(", ")));
    }
}

QObject *js_bridge::add_sprite(const QJSValue &path, const QJSValue &layer, const QJSValue &origin,
                               const QJSValue &x, const QJSValue &y, int source_line) {
    js_args a(QStringLiteral("add_sprite(path, layer, origin, x, y)"));
    const QString path_text = a.text(path, "path");
    const QString layer_text = a.text(layer, "layer");
    const QString origin_text = a.text(origin, "origin");
    const double x_value = a.number(x, "x");
    const double y_value = a.number(y, "y");

    if (!a.ok()) {
        eng_->throwError(QJSValue::TypeError, a.error());
        return nullptr;
    }

    return create_sprite(path_text, layer_text, origin_text, x_value, y_value, source_line);
}

QObject *js_bridge::create_sprite(const QString &path, const QString &layer, const QString &origin,
                                  double x, double y, int source_line) {
    const std::optional<layer_t> ly = to_layer(layer);
    const std::optional<origin_t> orr = to_origin(origin);

    if (!ly || !orr) {
        return nullptr;     // exception pending
    }

    const qsizetype index = sc_->add_sprite(path, *ly, *orr, x, y);
    sc_->sprites[index].source_line = source_line;

    return new js_sprite_proxy(this, index, generation_);
}

std::optional<layer_t> js_bridge::to_layer(const QString &s) {
    static const layer_t layers[] = {
        layer_t::background, layer_t::fail, layer_t::pass, layer_t::foreground, layer_t::overlay
    };

    for (int i = 0; i < layer_names.size(); ++i) {
        if (s.compare(layer_names.at(i), Qt::CaseInsensitive) == 0) {
            return layers[i];
        }
    }

    eng_->throwError(QJSValue::TypeError, unknown_name("layer", s, layer_names));
    return std::nullopt;
}

std::optional<origin_t> js_bridge::to_origin(const QString &s) {
    static const origin_t origins[] = {
        origin_t::top_left, origin_t::top_center, origin_t::top_right,
        origin_t::center_left, origin_t::center, origin_t::center_right,
        origin_t::bottom_left, origin_t::bottom_center, origin_t::bottom_right
    };

    for (int i = 0; i < origin_names.size(); ++i) {
        if (s.compare(origin_names.at(i), Qt::CaseInsensitive) == 0) {
            return origins[i];
        }
    }

    for (const QString &alias : center_aliases) {
        if (s.compare(alias, Qt::CaseInsensitive) == 0) {
            return origin_t::center;
        }
    }

    eng_->throwError(QJSValue::TypeError, unknown_name("origin", s, origin_names));
    return std::nullopt;
}

QObject *js_bridge::sprite(const QJSValue &path, const QJSValue &opts, int source_line) {
    const QString where = QStringLiteral("sprite(path, options)");
    js_options o(opts, where, sprite_keys);
    return sprite_from(where, path, o, source_line);
}

QObject *js_bridge::sprite_from(const QString &where, const QJSValue &path, js_options &o, int source_line) {
    js_args a(where);
    const QString path_text = a.text(path, "path");

    if (!a.ok()) {
        eng_->throwError(QJSValue::TypeError, a.error());
        return nullptr;
    }

    const QString layer = o.text("layer", QStringLiteral("Foreground"));
    const QString origin = o.text("origin", QStringLiteral("Center"));

    double position[2]{320.0, 240.0};
    o.numbers("position", 2, position);

    const double x = o.number("x", position[0]);
    const double y = o.number("y", position[1]);
    const QString name = o.text("name", QString());

    if (!o.ok()) {
        eng_->throwError(QJSValue::TypeError, o.error());
        return nullptr;
    }

    QObject *proxy = create_sprite(path_text, layer, origin, x, y, source_line);

    if (proxy) {
        sc_->sprites.back().name = name;
    }

    return proxy;
}

QObject *js_bridge::animation(const QJSValue &path, const QJSValue &opts, int source_line) {
    const QString where = QStringLiteral("animation(path, options)");
    js_options o(opts, where, sprite_keys + QStringList{
            QStringLiteral("frameCount"), QStringLiteral("frameDelay"), QStringLiteral("loopType")});

    if (!o.ok()) {
        eng_->throwError(QJSValue::TypeError, o.error());
        return nullptr;
    }

    const QJSValue count = opts.property("frameCount");
    const QJSValue delay = opts.property("frameDelay");

    if (!count.isNumber() || count.toNumber() < 1 || count.toNumber() != std::floor(count.toNumber())) {
        eng_->throwError(QJSValue::RangeError,
                         QStringLiteral("%1: frameCount must be a whole number of frames, at least 1 (got %2)")
                                 .arg(where, js_args::describe(count)));
        return nullptr;
    }

    if (!delay.isNumber() || !(delay.toNumber() > 0) || !std::isfinite(delay.toNumber())) {
        eng_->throwError(QJSValue::RangeError,
                         QStringLiteral("%1: frameDelay must be the milliseconds per frame, above 0 (got %2)")
                                 .arg(where, js_args::describe(delay)));
        return nullptr;
    }

    animation_loop_t loop_type = animation_loop_t::loop_forever;
    const QString text = o.text("loopType", QStringLiteral("LoopForever"));

    if (!o.ok()) {
        eng_->throwError(QJSValue::TypeError, o.error());
        return nullptr;
    }

    if (text.compare("LoopOnce", Qt::CaseInsensitive) == 0) {
        loop_type = animation_loop_t::loop_once;
    }
    else if (text.compare("LoopForever", Qt::CaseInsensitive) != 0) {
        eng_->throwError(QJSValue::TypeError,
                         QStringLiteral("Unknown loopType \"%1\".%2 Use \"LoopForever\" or \"LoopOnce\"").arg(
                                 text, did_you_mean(text, {QStringLiteral("LoopForever"), QStringLiteral("LoopOnce")})));
        return nullptr;
    }

    QObject *proxy = sprite_from(where, path, o, source_line);

    if (!proxy) {
        return nullptr;
    }

    ::sprite &s = sc_->sprites.back();
    s.frame_count = count.toInt();
    s.frame_delay = delay.toNumber();
    s.loop_type = loop_type;

    return proxy;
}

void js_bridge::sample(const QJSValue &time, const QJSValue &path, const QJSValue &opts, int source_line) {
    const QString where = QStringLiteral("sample(time, path, options)");

    js_args a(where);
    const int time_value = a.integer(time, "time");
    const QString path_text = a.text(path, "path");

    if (!a.ok()) {
        eng_->throwError(QJSValue::TypeError, a.error());
        return;
    }

    js_options o(opts, where, {QStringLiteral("layer"), QStringLiteral("volume")});

    const QString layer = o.text("layer", QStringLiteral("Foreground"));
    const double volume = o.number("volume", 100);

    if (!o.ok()) {
        eng_->throwError(QJSValue::TypeError, o.error());
        return;
    }

    if (layer.compare("Overlay", Qt::CaseInsensitive) == 0) {
        eng_->throwError(QJSValue::RangeError,
                         QStringLiteral("Samples can't use the Overlay layer; use Background, Fail, Pass or Foreground"));
        return;
    }

    const std::optional<layer_t> ly = to_layer(layer);

    if (!ly) {
        return;
    }

    if (volume < 0 || volume > 100) {
        eng_->throwError(QJSValue::RangeError,
                         QStringLiteral("%1: volume must be 0-100 (got %2)").arg(where).arg(volume));
        return;
    }

    ::sample smp;
    smp.time = time_value;
    smp.path = path_text;
    smp.source_line = source_line;
    smp.layer = *ly;
    smp.volume = qRound(volume);

    sc_->samples.push_back(smp);
}
