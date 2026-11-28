#pragma once

#include <QObject>
#include <QJSValue>
#include "core/scene.h"

class js_bridge;

// The JS-side handle for one sprite.
//
// Holds an index, never a sprite* / sprite&: creating more sprites can
// reallocate scene::sprites, which would leave a pointer dangling. The run
// generation makes handles kept from an earlier run (e.g. in a global) fail
// loudly instead of editing whatever sprite now has the same index.
//
// The handle passed to a loop()/trigger() callback is a proxy for that group:
// its commands go into the loop or trigger, and it can't nest another one.
class js_sprite_proxy : public QObject
{
        Q_OBJECT

public:
        enum class group_kind { none, loop, trigger };

        js_sprite_proxy(js_bridge *bridge, qsizetype index, quint64 generation,
                        group_kind group = group_kind::none, qsizetype group_index = -1)
                : bridge_(bridge), index_(index), generation_(generation),
                  group_(group), group_index_(group_index) {}

        // Every argument arrives as a raw JS value and is checked here, so a
        // mistake like "1000" or undefined for a time is an error instead of
        // a silent 0. The defaults let a call with missing arguments reach
        // these checks rather than failing in Qt with a generic message.

        // fade, scale and rotate: (options) or (easing, startTime, endTime, startValue, endValue).
        Q_INVOKABLE js_sprite_proxy *fade(const QJSValue &a = QJSValue(), const QJSValue &b = QJSValue(),
                                          const QJSValue &c = QJSValue(), const QJSValue &d = QJSValue(),
                                          const QJSValue &e = QJSValue());
        Q_INVOKABLE js_sprite_proxy *scale(const QJSValue &a = QJSValue(), const QJSValue &b = QJSValue(),
                                           const QJSValue &c = QJSValue(), const QJSValue &d = QJSValue(),
                                           const QJSValue &e = QJSValue());
        Q_INVOKABLE js_sprite_proxy *rotate(const QJSValue &a = QJSValue(), const QJSValue &b = QJSValue(),
                                            const QJSValue &c = QJSValue(), const QJSValue &d = QJSValue(),
                                            const QJSValue &e = QJSValue());

        // (options) or (easing, startTime, endTime, startX, startY, endX, endY).
        Q_INVOKABLE js_sprite_proxy *move(const QJSValue &a = QJSValue(), const QJSValue &b = QJSValue(),
                                          const QJSValue &c = QJSValue(), const QJSValue &d = QJSValue(),
                                          const QJSValue &e = QJSValue(), const QJSValue &f = QJSValue(),
                                          const QJSValue &g = QJSValue());

        // (options) or a constant colour: (easing, startTime, endTime, r, g, b).
        Q_INVOKABLE js_sprite_proxy *color(const QJSValue &a = QJSValue(), const QJSValue &b = QJSValue(),
                                           const QJSValue &c = QJSValue(), const QJSValue &d = QJSValue(),
                                           const QJSValue &e = QJSValue(), const QJSValue &f = QJSValue());

        Q_INVOKABLE js_sprite_proxy *moveX(const QJSValue &opts = QJSValue());
        Q_INVOKABLE js_sprite_proxy *moveY(const QJSValue &opts = QJSValue());
        Q_INVOKABLE js_sprite_proxy *scaleVec(const QJSValue &opts = QJSValue());
        Q_INVOKABLE js_sprite_proxy *flipH(const QJSValue &opts = QJSValue());
        Q_INVOKABLE js_sprite_proxy *flipV(const QJSValue &opts = QJSValue());
        Q_INVOKABLE js_sprite_proxy *additive(const QJSValue &opts = QJSValue());

        // L: body(l) adds commands, timed from `start`, played `count` times.
        Q_INVOKABLE js_sprite_proxy *loop(const QJSValue &start = QJSValue(), const QJSValue &count = QJSValue(),
                                          const QJSValue &body = QJSValue());

        // T: body(t) adds commands played when `trigger` fires between start
        // and end, timed from that moment. group 0 (or left out) means none.
        Q_INVOKABLE js_sprite_proxy *trigger(const QJSValue &trigger = QJSValue(), const QJSValue &start = QJSValue(),
                                             const QJSValue &end = QJSValue(), const QJSValue &body = QJSValue(),
                                             const QJSValue &group = QJSValue());

private:
        // The sprite, or nullptr (and a JS exception) if this handle is stale.
        sprite *target();

        // Stores the command, or throws a JS RangeError when its end is
        // before its start.
        void push(const command &cmd);

        // Parses `easing` into the command and push()es it.
        js_sprite_proxy *add(command cmd, const QString &easing);

        // fade, scale, rotate (5 args, positional form allowed) and moveX,
        // moveY (1 arg, the default value comes from the sprite's x or y).
        js_sprite_proxy *value_command(cmd_t type, double default_value, const QJSValueList &args);
        // flipH, flipV, additive.
        js_sprite_proxy *parameter(cmd_t type, const QJSValue &opts);

        // Throws a JS TypeError (and returns false) for an unknown easing.
        bool parse(const QString &text, easing_t &out);

        // Runs body with a group proxy; false (exception pending) on failure.
        bool run_group(const QJSValue &body, group_kind kind, qsizetype group_index);

        // Throws a JS error; returns `this` so commands can `return fail(...)`.
        js_sprite_proxy *fail(QJSValue::ErrorType type, const QString &message);

        js_bridge *bridge_;
        qsizetype index_;
        quint64 generation_;
        group_kind group_;
        qsizetype group_index_;
};
