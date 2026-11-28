#pragma once

#include "core/scene.h"
#include "scripting/js_sprite_proxy.h"

#include <QObject>
#include <QJSEngine>

#include <optional>

class js_options;

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
        //
        // Every argument but the line arrives as a raw JS value and is
        // checked here, so a mistake is an error instead of a silent 0 or
        // "undefined" path.
        Q_INVOKABLE QObject* add_sprite(const QJSValue& path, const QJSValue& layer, const QJSValue& origin,
                const QJSValue& x, const QJSValue& y, int source_line = 0);

        Q_INVOKABLE QObject* sprite(const QJSValue& path, const QJSValue& opts, int source_line = 0);

        // Like sprite(), plus opts.frameCount, opts.frameDelay (ms) and
        // opts.loopType ("LoopForever" or "LoopOnce").
        Q_INVOKABLE QObject* animation(const QJSValue& path, const QJSValue& opts, int source_line = 0);

        // A sound at `time`; opts.layer (not Overlay) and opts.volume (0-100).
        Q_INVOKABLE void sample(const QJSValue& time, const QJSValue& path, const QJSValue& opts, int source_line = 0);

    private:
        QJSEngine* eng_;
        scene* sc_;
        quint64 generation_ = 0;

        // nullopt (and a JS TypeError naming the closest match) for an
        // unknown name.
        std::optional<layer_t> to_layer(const QString& s);
        std::optional<origin_t> to_origin(const QString& s);

        // Adds a sprite whose arguments are already checked; nullptr (and a
        // JS error) for an unknown layer or origin.
        QObject* create_sprite(const QString& path, const QString& layer, const QString& origin,
                double x, double y, int source_line);

        // sprite() and animation(): reads the sprite's own options from `o`.
        QObject* sprite_from(const QString& where, const QJSValue& path, js_options& o, int source_line);

};
