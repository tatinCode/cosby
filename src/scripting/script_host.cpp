#include "scripting/script_host.h"

#include <QDebug>

script_host::script_host(QObject* parent) : QObject(parent){
    bridge_ = new js_bridge(&eng_, &sc_, &eng_);

    // The native bridge stays hidden; scripts use the SceneApi wrapper below,
    // which tells the bridge which script line created each sprite.
    eng_.globalObject().setProperty("__cosby_scene", eng_.newQObject(bridge_));

    //simple print() ???????
    //
    //not very simple now lmaooo
    auto* native = new NativeConsole(&eng_);
    connect(native, &NativeConsole::printed, this, &script_host::output);

    eng_.globalObject().setProperty("__native", eng_.newQObject(native));

    eng_.evaluate(
            QStringLiteral(R"JS(
            function print(){
             var out = "";
             for(var i = 0; i < arguments.length; ++i){
               if(i){
                   out += " ";
                }
                out += String(arguments[i]);
             }
             __native.print(out);
            }

            //console.* aliases:
            var console = { log: print, warn: print, error: print };

            //SceneApi: same API as the native bridge, plus the caller's line.
            //Stack frames look like "name@file:%3Cscript%3E:12"; the first
            //<script> frame is the line that called sprite().
            var SceneApi = (function(native){
                var script_frame = /(?:%3Cscript%3E|<script>):(\d+)\s*$/;

                function caller_line(){
                    var frames = String(new Error().stack || "").split("\n");

                    for(var i = 0; i < frames.length; ++i){
                        var match = script_frame.exec(frames[i]);
                        if(match){
                            return Number(match[1]);
                        }
                    }

                    return 0;
                }

                return Object.freeze({
                    sprite: function(path, options){
                        return native.sprite(path, options === undefined ? {} : options, caller_line());
                    },
                    add_sprite: function(path, layer, origin, x, y){
                        return native.add_sprite(path, layer, origin, x, y, caller_line());
                    }
                });
            })(__cosby_scene);
            )JS"),
            QStringLiteral("<bootstrap>")   //filename for error stacks
            );
}

std::optional<script_error> script_host::evaluate(const QString& source, const QString& preload_source){
    bridge_->begin_run();   //fresh scene; sprite handles from the last run go stale

    if(!preload_source.isEmpty()){
        QJSValue pre = eng_.evaluate(preload_source, "<preload>");

        if(pre.isError()){
            return script_error{pre.toString(), pre.property("lineNumber").toInt(), true};
        }
    }

    QJSValue res = eng_.evaluate(source, "<script>");

    if(res.isError()){
        return script_error{res.toString(), res.property("lineNumber").toInt(), false};
    }

    return std::nullopt;
}

QString script_host::run(const QString& source, const QString& preload_source){
    const auto error = evaluate(source, preload_source);

    if(!error.has_value()){
        return {};
    }

    // One multi-arg call: chained .arg() would substitute into a message containing "%3".
    return QString("%1: %2 (line %3)").arg(
            error->in_preload ? QStringLiteral("Preload error") : QStringLiteral("Error"),
            error->message,
            QString::number(error->line));
}


