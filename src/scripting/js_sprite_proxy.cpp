#include <QJSValue>
#include <QVariant>
#include <QDebug>

#include "scripting/js_sprite_proxy.h"
#include "scripting/js_bridge.h"
#include "scripting/js_options.h"
#include "core/name_hint.h"

#include <QJSEngine>
#include <QRegularExpression>

sprite *js_sprite_proxy::target() {
    sprite *s = bridge_ ? bridge_->sprite_for(index_, generation_) : nullptr;

    if (!s) {
        if (QJSEngine *engine = qjsEngine(this)) {
            engine->throwError(QJSValue::ReferenceError,
                               QStringLiteral("This sprite belongs to an earlier run of the script"));
        }
    }

    return s;
}

namespace {
    // The script's names for the commands, for messages.
    QString command_name(cmd_t type) {
        switch (type) {
            case cmd_t::fade:      return QStringLiteral("fade");
            case cmd_t::move:      return QStringLiteral("move");
            case cmd_t::scale:     return QStringLiteral("scale");
            case cmd_t::rotate:    return QStringLiteral("rotate");
            case cmd_t::color:     return QStringLiteral("color");
            case cmd_t::move_x:    return QStringLiteral("moveX");
            case cmd_t::move_y:    return QStringLiteral("moveY");
            case cmd_t::scale_vec: return QStringLiteral("scaleVec");
            case cmd_t::flip_h:    return QStringLiteral("flipH");
            case cmd_t::flip_v:    return QStringLiteral("flipV");
            case cmd_t::additive:  return QStringLiteral("additive");
        }
        return QStringLiteral("command");
    }

    bool is_parameter(cmd_t type) {
        return type == cmd_t::flip_h || type == cmd_t::flip_v || type == cmd_t::additive;
    }

    // fade("Linear", 0, 1000, 0, 1) rather than fade({ ... }).
    bool is_positional(const QJSValue &first) {
        return first.isString() || first.isNumber();
    }

    QString options_where(cmd_t type) {
        return QStringLiteral("%1(options)").arg(command_name(type));
    }

    const QStringList timed_keys{QStringLiteral("startTime"), QStringLiteral("endTime")};
    const QStringList eased_keys = QStringList{QStringLiteral("easing")} + timed_keys;
    const QStringList value_keys = eased_keys + QStringList{QStringLiteral("startValue"), QStringLiteral("endValue")};
    const QStringList move_keys = eased_keys + QStringList{
            QStringLiteral("startPosition"), QStringLiteral("endPosition"),
            QStringLiteral("startX"), QStringLiteral("startY"),
            QStringLiteral("endX"), QStringLiteral("endY"),
            QStringLiteral("x"), QStringLiteral("y")};
    const QStringList color_keys = value_keys + QStringList{
            QStringLiteral("r"), QStringLiteral("g"), QStringLiteral("b")};
}

void js_sprite_proxy::push(const command &cmd) {
    // flipH/flipV/additive: an end at or before the start means "from then on".
    if (!is_parameter(cmd.type) && cmd.t1 < cmd.t0) {
        fail(QJSValue::RangeError, QStringLiteral("%1(): endTime (%2) is before startTime (%3)")
                .arg(command_name(cmd.type)).arg(cmd.t1).arg(cmd.t0));
        return;
    }

    sprite *s = target();

    if (!s) {
        return;
    }

    switch (group_) {
        case group_kind::none:
            s->cmds.push_back(cmd);
            break;

        case group_kind::loop:
            if (group_index_ >= 0 && group_index_ < s->loops.size()) {
                s->loops[group_index_].cmds.push_back(cmd);
            }
            break;

        case group_kind::trigger:
            if (group_index_ >= 0 && group_index_ < s->triggers.size()) {
                s->triggers[group_index_].cmds.push_back(cmd);
            }
            break;
    }
}

js_sprite_proxy *js_sprite_proxy::fail(QJSValue::ErrorType type, const QString &message) {
    if (QJSEngine *engine = qjsEngine(this)) {
        engine->throwError(type, message);
    }

    return this;
}

js_sprite_proxy *js_sprite_proxy::add(command cmd, const QString &easing) {
    if (parse(easing, cmd.easing)) {
        push(cmd);
    }

    return this;
}

bool js_sprite_proxy::parse(const QString &text, easing_t &out) {
    if (parse_easing(text, out)) {
        return true;
    }

    const QString hint = did_you_mean(text, easing_names());
    fail(QJSValue::TypeError, hint.isEmpty()
         ? QStringLiteral("Unknown easing \"%1\". Use a name such as \"QuadOut\" or osu!'s number (0-%2)")
                 .arg(text).arg(easing_count - 1)
         : QStringLiteral("Unknown easing \"%1\".%2").arg(text, hint));
    return false;
}

js_sprite_proxy *js_sprite_proxy::value_command(cmd_t type, double default_value, const QJSValueList &args) {
    command cmd;
    cmd.type = type;
    QString easing;

    if (args.size() == 5 && is_positional(args.at(0))) {
        js_args a(QStringLiteral("%1(easing, startTime, endTime, startValue, endValue)").arg(command_name(type)));

        easing = a.text(args.at(0), "easing", true);
        cmd.t0 = a.integer(args.at(1), "startTime");
        cmd.t1 = a.integer(args.at(2), "endTime");
        cmd.v0 = a.number(args.at(3), "startValue");
        cmd.v1 = a.number(args.at(4), "endValue");

        if (!a.ok()) {
            return fail(QJSValue::TypeError, a.error());
        }

        return add(cmd, easing);
    }

    js_options o(args.value(0), options_where(type), value_keys);

    easing = o.text("easing", QStringLiteral("Linear"), true);
    cmd.t0 = o.integer("startTime", 0);
    cmd.t1 = o.integer("endTime", cmd.t0);      // instant when left out

    if (type == cmd_t::move_x || type == cmd_t::move_y) {
        const sprite *s = target();
        if (!s) {
            return this;
        }
        default_value = type == cmd_t::move_y ? s->y : s->x;
    }

    cmd.v0 = o.number("startValue", default_value);
    cmd.v1 = o.number("endValue", default_value);

    if (!o.ok()) {
        return fail(QJSValue::TypeError, o.error());
    }

    return add(cmd, easing);
}

js_sprite_proxy *js_sprite_proxy::fade(const QJSValue &a, const QJSValue &b, const QJSValue &c,
                                       const QJSValue &d, const QJSValue &e) {
    return value_command(cmd_t::fade, 0.0, {a, b, c, d, e});
}

js_sprite_proxy *js_sprite_proxy::scale(const QJSValue &a, const QJSValue &b, const QJSValue &c,
                                        const QJSValue &d, const QJSValue &e) {
    return value_command(cmd_t::scale, 1.0, {a, b, c, d, e});
}

js_sprite_proxy *js_sprite_proxy::rotate(const QJSValue &a, const QJSValue &b, const QJSValue &c,
                                         const QJSValue &d, const QJSValue &e) {
    return value_command(cmd_t::rotate, 0.0, {a, b, c, d, e});
}

js_sprite_proxy *js_sprite_proxy::moveX(const QJSValue &opts) {
    return value_command(cmd_t::move_x, 0.0, {opts});
}

js_sprite_proxy *js_sprite_proxy::moveY(const QJSValue &opts) {
    return value_command(cmd_t::move_y, 0.0, {opts});
}

js_sprite_proxy *js_sprite_proxy::move(const QJSValue &a, const QJSValue &b, const QJSValue &c,
                                       const QJSValue &d, const QJSValue &e, const QJSValue &f,
                                       const QJSValue &g) {
    command cmd;
    cmd.type = cmd_t::move;
    QString easing;

    if (is_positional(a)) {
        js_args p(QStringLiteral("move(easing, startTime, endTime, startX, startY, endX, endY)"));

        easing = p.text(a, "easing", true);
        cmd.t0 = p.integer(b, "startTime");
        cmd.t1 = p.integer(c, "endTime");
        cmd.x0 = p.number(d, "startX");
        cmd.y0 = p.number(e, "startY");
        cmd.x1 = p.number(f, "endX");
        cmd.y1 = p.number(g, "endY");

        if (!p.ok()) {
            return fail(QJSValue::TypeError, p.error());
        }

        return add(cmd, easing);
    }

    js_options o(a, options_where(cmd.type), move_keys);

    easing = o.text("easing", QStringLiteral("Linear"), true);
    cmd.t0 = o.integer("startTime", 0);
    cmd.t1 = o.integer("endTime", cmd.t0);

    double start[2]{};
    if (!o.numbers("startPosition", 2, start)) {
        start[0] = o.number("startX", o.number("x", 0.0));
        start[1] = o.number("startY", o.number("y", 0.0));
    }

    double end[2]{};
    if (!o.numbers("endPosition", 2, end)) {
        end[0] = o.number("endX", start[0]);
        end[1] = o.number("endY", start[1]);
    }

    if (!o.ok()) {
        return fail(QJSValue::TypeError, o.error());
    }

    cmd.x0 = start[0];
    cmd.y0 = start[1];
    cmd.x1 = end[0];
    cmd.y1 = end[1];

    return add(cmd, easing);
}

js_sprite_proxy *js_sprite_proxy::scaleVec(const QJSValue &opts) {
    command cmd;
    cmd.type = cmd_t::scale_vec;

    js_options o(opts, options_where(cmd.type), value_keys);

    const QString easing = o.text("easing", QStringLiteral("Linear"), true);
    cmd.t0 = o.integer("startTime", 0);
    cmd.t1 = o.integer("endTime", cmd.t0);

    double start[2]{1.0, 1.0};
    o.numbers("startValue", 2, start);
    double end[2]{1.0, 1.0};
    o.numbers("endValue", 2, end);

    if (!o.ok()) {
        return fail(QJSValue::TypeError, o.error());
    }

    cmd.x0 = start[0];
    cmd.y0 = start[1];
    cmd.x1 = end[0];
    cmd.y1 = end[1];

    return add(cmd, easing);
}

js_sprite_proxy *js_sprite_proxy::color(const QJSValue &a, const QJSValue &b, const QJSValue &c,
                                        const QJSValue &d, const QJSValue &e, const QJSValue &f) {
    command cmd;
    cmd.type = cmd_t::color;
    QString easing;

    if (is_positional(a)) {
        // A constant colour.
        js_args p(QStringLiteral("color(easing, startTime, endTime, r, g, b)"));

        easing = p.text(a, "easing", true);
        cmd.t0 = p.integer(b, "startTime");
        cmd.t1 = p.integer(c, "endTime");
        cmd.r0 = cmd.r1 = p.number(d, "r");
        cmd.g0 = cmd.g1 = p.number(e, "g");
        cmd.b0 = cmd.b1 = p.number(f, "b");

        if (!p.ok()) {
            return fail(QJSValue::TypeError, p.error());
        }

        return add(cmd, easing);
    }

    js_options o(a, options_where(cmd.type), color_keys);

    easing = o.text("easing", QStringLiteral("Linear"), true);
    cmd.t0 = o.integer("startTime", 0);
    cmd.t1 = o.integer("endTime", cmd.t0);

    // White unless given; the end defaults to the start.
    double start[3]{255.0, 255.0, 255.0};
    o.numbers("startValue", 3, start);

    double end[3]{start[0], start[1], start[2]};

    if (!o.numbers("endValue", 3, end) && (o.has("r") || o.has("g") || o.has("b"))) {
        // r/g/b: a constant colour.
        end[0] = start[0] = o.number("r", start[0]);
        end[1] = start[1] = o.number("g", start[1]);
        end[2] = start[2] = o.number("b", start[2]);
    }

    if (!o.ok()) {
        return fail(QJSValue::TypeError, o.error());
    }

    cmd.r0 = start[0];
    cmd.g0 = start[1];
    cmd.b0 = start[2];
    cmd.r1 = end[0];
    cmd.g1 = end[1];
    cmd.b1 = end[2];

    return add(cmd, easing);
}

js_sprite_proxy *js_sprite_proxy::parameter(cmd_t type, const QJSValue &opts) {
    js_options o(opts, options_where(type), timed_keys);

    command cmd;
    cmd.type = type;
    // endTime <= startTime means "from startTime on", so 0 is a fine default.
    cmd.t0 = o.integer("startTime", 0);
    cmd.t1 = o.integer("endTime", 0);

    if (!o.ok()) {
        return fail(QJSValue::TypeError, o.error());
    }

    push(cmd);
    return this;
}

js_sprite_proxy *js_sprite_proxy::flipH(const QJSValue &opts) {
    return parameter(cmd_t::flip_h, opts);
}

js_sprite_proxy *js_sprite_proxy::flipV(const QJSValue &opts) {
    return parameter(cmd_t::flip_v, opts);
}

js_sprite_proxy *js_sprite_proxy::additive(const QJSValue &opts) {
    return parameter(cmd_t::additive, opts);
}

namespace {
    // osu!'s trigger names: Passing, Failing, or HitSound followed by an
    // optional sample set, additions sample set, addition and custom sample
    // set number ("HitSoundClap", "HitSoundSoftWhistle", "HitSound1").
    bool is_trigger_name(const QString &name) {
        static const QRegularExpression pattern(QStringLiteral(
                "^(Passing|Failing|HitSound(All|Normal|Soft|Drum)?(All|Normal|Soft|Drum)?(Whistle|Finish|Clap)?\\d*)$"));

        return pattern.match(name).hasMatch();
    }
}

bool js_sprite_proxy::run_group(const QJSValue &body, group_kind kind, qsizetype group_index) {
    QJSEngine *engine = qjsEngine(this);

    if (!engine) {
        return false;
    }

    // No parent: the JS garbage collector owns it.
    auto *group = new js_sprite_proxy(bridge_, index_, generation_, kind, group_index);
    const QJSValue result = body.call({engine->newQObject(group)});

    if (result.isError()) {
        // Pass the script's own error on, with its line.
        engine->throwError(result);
        return false;
    }

    return true;
}

js_sprite_proxy *js_sprite_proxy::loop(const QJSValue &start_value, const QJSValue &count_value,
                                       const QJSValue &body) {
    if (group_ != group_kind::none) {
        return fail(QJSValue::TypeError, QStringLiteral("A loop or trigger can't contain another loop"));
    }

    js_args a(QStringLiteral("loop(start, count, body)"));
    const int start = a.integer(start_value, "start");
    const int count = a.integer(count_value, "count");

    if (!a.ok()) {
        return fail(QJSValue::TypeError, a.error());
    }

    if (!body.isCallable()) {
        return fail(QJSValue::TypeError, QStringLiteral("loop(start, count, body): body must be a function, e.g. l => l.fade(...) (got %1)")
                .arg(js_args::describe(body)));
    }

    if (count < 1) {
        return fail(QJSValue::RangeError, QStringLiteral("loop(start, count, body): count is the number of passes and must be at least 1 (got %1)").arg(count));
    }

    sprite *s = target();

    if (!s) {
        return this;
    }

    loop_group group;
    group.start = start;
    group.count = count;
    s->loops.push_back(group);

    // The callback may create sprites, which moves `s`; only the index is kept.
    run_group(body, group_kind::loop, s->loops.size() - 1);
    return this;
}

js_sprite_proxy *js_sprite_proxy::trigger(const QJSValue &name, const QJSValue &start_value,
                                          const QJSValue &end_value, const QJSValue &body,
                                          const QJSValue &group_value) {
    if (group_ != group_kind::none) {
        return fail(QJSValue::TypeError, QStringLiteral("A loop or trigger can't contain another trigger"));
    }

    js_args a(QStringLiteral("trigger(name, start, end, body, group)"));
    const QString trigger = a.text(name, "name");
    const int start = a.integer(start_value, "start");
    const int end = a.integer(end_value, "end");
    const int group = group_value.isUndefined() ? 0 : a.integer(group_value, "group");

    if (!a.ok()) {
        return fail(QJSValue::TypeError, a.error());
    }

    if (!is_trigger_name(trigger)) {
        const QString hint = did_you_mean(trigger, {QStringLiteral("Passing"), QStringLiteral("Failing"), QStringLiteral("HitSound")});
        return fail(QJSValue::TypeError,
                    QStringLiteral("Unknown trigger \"%1\".%2 Use Passing, Failing or HitSound..., "
                                   "e.g. \"HitSoundClap\" or \"HitSoundSoftWhistle\"").arg(trigger, hint));
    }

    if (!body.isCallable()) {
        return fail(QJSValue::TypeError, QStringLiteral("trigger(name, start, end, body): body must be a function, e.g. t => t.scale(...) (got %1)")
                .arg(js_args::describe(body)));
    }

    if (end < start) {
        return fail(QJSValue::RangeError, QStringLiteral("trigger(name, start, end, body): end (%1) is before start (%2)").arg(end).arg(start));
    }

    if (group < 0) {
        return fail(QJSValue::RangeError, QStringLiteral("trigger(...): group must be 0 (none) or a positive number (got %1)").arg(group));
    }

    sprite *s = target();

    if (!s) {
        return this;
    }

    trigger_group entry;
    entry.trigger = trigger;
    entry.start = start;
    entry.end = end;
    entry.group = group;
    s->triggers.push_back(entry);

    run_group(body, group_kind::trigger, s->triggers.size() - 1);
    return this;
}
