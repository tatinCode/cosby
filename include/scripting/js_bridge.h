#pragma once

#include "core/scene.h"
#include "scripting/js_sprite_proxy.h"

#include <QObject>
#include <QJSEngine>

class js_bridge : public QObject{
    Q_OBJECT

    public:
        js_bridge(QJSEngine* eng, scene* sc, QObject* parent = nullptr);

        Q_INVOKABLE QObject* add_sprite(const QString& path, const QString& layer,
                const QString& origin, double x, double y);

        Q_INVOKABLE QObject* sprite(const QString& path, const QJSValue& opts);

    private:
        QJSEngine* eng_;
        scene* sc_;

        layer_t to_layer(const QString& s);
        origin_t to_origin(const QString& s);

};
