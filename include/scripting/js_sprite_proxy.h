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

        Q_INVOKABLE js_sprite_proxy *fade(const QString &easing, int t0, int t1, double a0, double a1);
        Q_INVOKABLE js_sprite_proxy *move(const QString &easing, int t0, int t1,
                                          double x0, double y0, double x1, double y1);
        Q_INVOKABLE js_sprite_proxy *scale(const QString &easing, int t0, int t1,
                                           double s0, double s1);
        Q_INVOKABLE js_sprite_proxy *rotate(const QString &easing, int t0, int t1,
                                            double r0, double r1);
        Q_INVOKABLE js_sprite_proxy *color(const QString &easing, int t0, int t1,
                                           double r, double g, double b);

        Q_INVOKABLE js_sprite_proxy *fade(const QJSValue &opts);
        Q_INVOKABLE js_sprite_proxy *move(const QJSValue &opts);
        Q_INVOKABLE js_sprite_proxy *scale(const QJSValue &opts);
        Q_INVOKABLE js_sprite_proxy *rotate(const QJSValue &opts);
        Q_INVOKABLE js_sprite_proxy *color(const QJSValue &opts);

        Q_INVOKABLE js_sprite_proxy *moveX(const QJSValue &opts);
        Q_INVOKABLE js_sprite_proxy *moveY(const QJSValue &opts);
        Q_INVOKABLE js_sprite_proxy *scaleVec(const QJSValue &opts);
        Q_INVOKABLE js_sprite_proxy *flipH(const QJSValue &opts);
        Q_INVOKABLE js_sprite_proxy *flipV(const QJSValue &opts);
        Q_INVOKABLE js_sprite_proxy *additive(const QJSValue &opts);

        // L: body(l) adds commands, timed from `start`, played `count` times.
        Q_INVOKABLE js_sprite_proxy *loop(int start, int count, const QJSValue &body);

        // T: body(t) adds commands played when `trigger` fires between start
        // and end, timed from that moment. group 0 means none.
        Q_INVOKABLE js_sprite_proxy *trigger(const QString &trigger, int start, int end,
                                             const QJSValue &body, int group = 0);

private:
        // The sprite, or nullptr (and a JS exception) if this handle is stale.
        sprite *target();

        void push(const command &cmd);

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
