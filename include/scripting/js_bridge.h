#pragma once

#include "core/scene.h"
#include "scripting/js_sprite_proxy.h"

#include <QObject>
#include <QJSEngine>

class NativeConsole : public QObject{
    Q_OBJECT

    public:
        using QObject::QObject;
        
        Q_INVOKABLE void print(const QString& s){
            qInfo().noquote() << s;
            emit printed(s);
        }

    signals:
        void printed(const QString& text);
};


class js_bridge : public QObject{
    Q_OBJECT

    public:
        js_bridge(QJSEngine* eng, scene* sc, QObject* parent = nullptr);

        // Clears the scene for a new run; handles from older runs go stale.
        void begin_run();

        // nullptr when `generation` is not the current run or index is out of range.
        ::sprite* sprite_for(qsizetype index, quint64 generation);

        // Called by the JS SceneApi wrapper (see script_host.cpp), which adds the
        // script line of the call. Proxies are returned without a parent, so the
        // JS garbage collector owns them.
        Q_INVOKABLE QObject* add_sprite(const QString& path, const QString& layer,
                const QString& origin, double x, double y, int source_line = 0);

        Q_INVOKABLE QObject* sprite(const QString& path, const QJSValue& opts, int source_line = 0);

    private:
        QJSEngine* eng_;
        scene* sc_;
        quint64 generation_ = 0;

        layer_t to_layer(const QString& s);
        origin_t to_origin(const QString& s);

};
