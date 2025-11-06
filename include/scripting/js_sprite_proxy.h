#pragma once

#include <QObject>
#include "core/scene.h"

class js_sprite_proxy : public QObject{
    Q_OBJECT

    public:
        explicit js_sprite_proxy(sprite* s, QObject* parent=nullptr) : QObject(parent), s_(s) {}

        Q_INVOKABLE js_sprite_proxy* fade(const QString& easing, int t0, int t1, double a0, double a1);
        Q_INVOKABLE js_sprite_proxy* move(const QString& easing, int t0, int t1,
                double x0, double y0, double x1, double y1);
        Q_INVOKABLE js_sprite_proxy* scale(const QString& easing, int t0, int t1,
                double s0, double s1);
        Q_INVOKABLE js_sprite_proxy* rotate(const QString& easing, int t0, int t1,
                double r0, double r1);
        Q_INVOKABLE js_sprite_proxy* color(const QString& easing, int t0, int t1,
                double r, double g, double b);

    private:
        sprite* s_;

};
