#include <QJSValue>
#include <QVariant>
#include <QDebug>

#include "scripting/js_sprite_proxy.h"
#include "scripting/js_bridge.h"

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

void js_sprite_proxy::push(const command &cmd) {
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

bool js_sprite_proxy::parse(const QString &text, easing_t &out) {
    if (parse_easing(text, out)) {
        return true;
    }

    fail(QJSValue::TypeError,
         QStringLiteral("Unknown easing \"%1\". Use a name such as \"QuadOut\" or osu!'s number (0-%2)")
                 .arg(text).arg(easing_count - 1));
    return false;
}

js_sprite_proxy *js_sprite_proxy::fade(const QString &easing, int t0, int t1,
                                       double a0, double a1) {
    easing_t ease{};
    if (!parse(easing, ease)) {
        return this;
    }

    command cmd;

    cmd.type = cmd_t::fade;
    cmd.easing = ease;
    cmd.t0 = t0;
    cmd.t1 = t1;
    cmd.v0 = a0;
    cmd.v1 = a1;

    push(cmd);
    return this;
}

js_sprite_proxy *js_sprite_proxy::move(const QString &easing, int t0, int t1,
                                       double x0, double y0, double x1, double y1){
    easing_t ease{};
    if (!parse(easing, ease)) {
        return this;
    }

    command cmd;
    cmd.type = cmd_t::move;
    cmd.easing = ease;
    cmd.t0 = t0;
    cmd.t1 = t1;
    cmd.x0 = x0;
    cmd.y0 = y0;
    cmd.x1 = x1;
    cmd.y1 = y1;

    push(cmd);
    return this;
}

js_sprite_proxy *js_sprite_proxy::scale(const QString &easing, int t0, int t1,
                                        double s0, double s1){
    easing_t ease{};
    if (!parse(easing, ease)) {
        return this;
    }

    command cmd;

    cmd.type = cmd_t::scale;
    cmd.easing = ease;
    cmd.t0 = t0;
    cmd.t1 = t1;
    cmd.v0 = s0;
    cmd.v1 = s1;

    push(cmd);
    return this;
}

js_sprite_proxy *js_sprite_proxy::rotate(const QString &easing, int t0, int t1,
                                         double r0, double r1){
    easing_t ease{};
    if (!parse(easing, ease)) {
        return this;
    }

    command cmd;

    cmd.type = cmd_t::rotate;
    cmd.easing = ease;
    cmd.t0 = t0;
    cmd.t1 = t1;
    cmd.v0 = r0;
    cmd.v1 = r1;

    push(cmd);
    return this;
}

js_sprite_proxy *js_sprite_proxy::color(const QString &easing, int t0, int t1,
                                        double r, double g, double b){
    easing_t ease{};
    if (!parse(easing, ease)) {
        return this;
    }

    command cmd;

    cmd.type = cmd_t::color;
    cmd.easing = ease;
    cmd.t0 = t0;
    cmd.t1 = t1;

    cmd.r0 = r;
    cmd.g0 = g;
    cmd.b0 = b;

    cmd.r1 = r;
    cmd.g1 = g;
    cmd.b1 = b;

    push(cmd);
    return this;
}

static QString readEasing(const QJSValue &opts){
    if (opts.hasProperty("easing"))
    {
        return opts.property("easing").toString();
    }

    return QStringLiteral("Linear");
}

static int readInt(const QJSValue &obj, const char *key, int def = 0){
    return obj.hasProperty(key) ? obj.property(key).toInt() : def;
}

static double readNumber(const QJSValue &obj, const char *key, double def = 0.0)
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

js_sprite_proxy *js_sprite_proxy::fade(const QJSValue &opts) {
    const QString easing = readEasing(opts);
    const int t0 = readInt(opts, "startTime", 0);
    const int t1 = readInt(opts, "endTime", 0);

    const double a0 = readNumber(opts, "startValue", 0.0);
    const double a1 = readNumber(opts, "endValue", 0.0);

    return fade(easing, t0, t1, a0, a1);
}

js_sprite_proxy *js_sprite_proxy::move(const QJSValue &opts)
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

js_sprite_proxy *js_sprite_proxy::scale(const QJSValue &opts)
{
    const QString easing = readEasing(opts);

    const int t0 = readInt(opts, "startTime", 0);
    const int t1 = readInt(opts, "endTime", 0);

    const double v0 = readNumber(opts, "startValue", 1.0);
    const double v1 = readNumber(opts, "endValue", 1.0);

    return scale(easing, t0, t1, v0, v1);
}

js_sprite_proxy *js_sprite_proxy::rotate(const QJSValue &opts) {
    const QString easing = readEasing(opts);

    const int t0 = readInt(opts, "startTime", 0);
    const int t1 = readInt(opts, "endTime", 0);

    const double v0 = readNumber(opts, "startValue", 0.0);
    const double v1 = readNumber(opts, "endValue", 0.0);

    return rotate(easing, t0, t1, v0, v1);
}

js_sprite_proxy *js_sprite_proxy::color(const QJSValue &opts)
{
    const QString easing = readEasing(opts);

    const int t0 = readInt(opts, "startTime", 0);
    const int t1 = readInt(opts, "endTime", 0);

    //set default color to white;
    double r0 = 255.0; 
    double g0 = 255.0;
    double b0 = 255.0;


    //startvalue: [r, g, b]
    if(opts.hasProperty("startValue")){
        QJSValue arr = opts.property("startValue");

        if(arr.isArray()){
            if(arr.property(0).isNumber()){
                r0 = arr.property(0).toNumber();
            }
            if(arr.property(1).isNumber()){
                g0 = arr.property(1).toNumber();
            }
            if(arr.property(2).isNumber()){
                b0 = arr.property(2).toNumber();
            }
        }
    }

    //end defautls to start values unless explicitly provided
    //
    double r1 = r0;
    double g1 = g0;
    double b1 = b0;

    //endvalue: [r, g, b]
    if(opts.hasProperty("endValue")){
        QJSValue arr = opts.property("endValue");

        if(arr.isArray()){
            if(arr.property(0).isNumber()){
                r1 = arr.property(0).toNumber();
            }
            if(arr.property(1).isNumber()){
                g1 = arr.property(1).toNumber();
            }
            if(arr.property(2).isNumber()){
                b1 = arr.property(2).toNumber();
            }
        }
    }
    else if(opts.hasProperty("r") || opts.hasProperty("g") || opts.hasProperty("b")){
        r0 = readNumber(opts, "r", r0);
        g0 = readNumber(opts, "g", g0);
        b0 = readNumber(opts, "b", b0);

        r1 = r0;
        g1 = g0;
        b1 = b0;
    }

    command cmd;
    
    cmd.type = cmd_t::color;

    if (!parse(easing, cmd.easing)) {
        return this;
    }

    cmd.t0 = t0;
    cmd.t1 = t1;
    cmd.r0 = r0;
    cmd.g0 = g0;
    cmd.b0 = b0;
    cmd.r1 = r1;
    cmd.g1 = g1;
    cmd.b1 = b1;

    push(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::moveX(const QJSValue &opts)
{
    const sprite *s = target();
    if (!s)
    {
        return this;
    }
    const sprite &base = *s;

    command cmd;

    cmd.type = cmd_t::move_x;
    if (!parse(readEasing(opts), cmd.easing)) {
        return this;
    }

    cmd.t0 = readInt(opts, "startTime", 0);
    cmd.t1 = readInt(opts, "endTime", 0);
    cmd.v0 = readNumber(opts, "startValue", base.x);
    cmd.v1 = readNumber(opts, "endValue", base.x);

    push(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::moveY(const QJSValue &opts)
{
    const sprite *s = target();
    if (!s)
    {
        return this;
    }
    const sprite &base = *s;

    command cmd;

    cmd.type = cmd_t::move_y;
    if (!parse(readEasing(opts), cmd.easing)) {
        return this;
    }

    cmd.t0 = readInt(opts, "startTime", 0);
    cmd.t1 = readInt(opts, "endTime", 0);
    cmd.v0 = readNumber(opts, "startValue", base.y);
    cmd.v1 = readNumber(opts, "endValue", base.y);

    push(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::scaleVec(const QJSValue &opts)
{
    command cmd;

    cmd.type = cmd_t::scale_vec;

    if (!parse(readEasing(opts), cmd.easing)) {
        return this;
    }

    cmd.t0 = readInt(opts, "startTime", 0);
    cmd.t1 = readInt(opts, "endTime", 0);

    double sx0 = 1.0, sy0 = 1.0;
    double sx1 = 1.0, sy1 = 1.0;

    readArray2(opts, "startValue", sx0, sy0);
    readArray2(opts, "endValue", sx1, sy1);

    cmd.x0 = sx0;
    cmd.y0 = sy0;
    cmd.x1 = sx1;
    cmd.y1 = sy1;

    push(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::flipH(const QJSValue &opts)
{
    command cmd;

    cmd.type = cmd_t::flip_h;
    cmd.t0 = readInt(opts, "startTime", 0);
    cmd.t1 = readInt(opts, "endTime", 0);

    push(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::flipV(const QJSValue &opts)
{
    command cmd;

    cmd.type = cmd_t::flip_v;
    cmd.t0 = readInt(opts, "startTime", 0);
    cmd.t1 = readInt(opts, "endTime", 0);

    push(cmd);

    return this;
}

js_sprite_proxy *js_sprite_proxy::additive(const QJSValue &opts)
{
    command cmd;

    cmd.type = cmd_t::additive;
    cmd.t0 = readInt(opts, "startTime", 0);
    cmd.t1 = readInt(opts, "endTime", 0);

    push(cmd);

    return this;
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

js_sprite_proxy *js_sprite_proxy::loop(int start, int count, const QJSValue &body) {
    if (group_ != group_kind::none) {
        return fail(QJSValue::TypeError, QStringLiteral("A loop or trigger can't contain another loop"));
    }

    if (!body.isCallable()) {
        return fail(QJSValue::TypeError, QStringLiteral("loop(start, count, body): body must be a function, e.g. l => l.fade(...)"));
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

js_sprite_proxy *js_sprite_proxy::trigger(const QString &trigger, int start, int end,
                                          const QJSValue &body, int group) {
    if (group_ != group_kind::none) {
        return fail(QJSValue::TypeError, QStringLiteral("A loop or trigger can't contain another trigger"));
    }

    if (!is_trigger_name(trigger)) {
        return fail(QJSValue::TypeError,
                    QStringLiteral("Unknown trigger \"%1\". Use Passing, Failing or HitSound..., "
                                   "e.g. \"HitSoundClap\" or \"HitSoundSoftWhistle\"").arg(trigger));
    }

    if (!body.isCallable()) {
        return fail(QJSValue::TypeError, QStringLiteral("trigger(name, start, end, body): body must be a function, e.g. t => t.scale(...)"));
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
