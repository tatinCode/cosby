#include "scripting/script_host.h"

#include <QRegularExpression>

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
                    },
                    animation: function(path, options){
                        return native.animation(path, options === undefined ? {} : options, caller_line());
                    },
                    sample: function(time, path, options){
                        native.sample(time, path, options === undefined ? {} : options, caller_line());
                    }
                });
            })(__cosby_scene);
            )JS"),
            QStringLiteral("<bootstrap>")   //filename for error stacks
            );
}

namespace {
    /**
     * The line in `file` where the error happened. Errors thrown by the
     * SceneApi wrapper or the native bridge (a bad option, say) point into
     * <bootstrap>; the first `file` frame of the stack is the script line
     * that made the call.
     */
    int error_line(const QJSValue& error, const QString& file){
        const QString encoded = QString(file).replace('<', "%3C").replace('>', "%3E");
        const QString error_file = error.property("fileName").toString();

        if(error_file.endsWith(file) || error_file.endsWith(encoded)){
            return error.property("lineNumber").toInt();
        }

        // Frames look like "name@file:%3Cscript%3E:12".
        const QRegularExpression frame(QStringLiteral("(?:%1|%2):(\\d+)\\s*$").arg(
                QRegularExpression::escape(file), QRegularExpression::escape(encoded)));

        for(const QString& line : error.property("stack").toString().split('\n')){
            const auto match = frame.match(line);

            if(match.hasMatch()){
                return match.captured(1).toInt();
            }
        }

        return error.property("lineNumber").toInt();
    }
}

std::optional<script_error> script_host::evaluate(const QString& source, const QString& preload_source){
    bridge_->begin_run();   //fresh scene; sprite handles from the last run go stale

    if(!preload_source.isEmpty()){
        QJSValue pre = eng_.evaluate(preload_source, "<preload>");

        if(pre.isError()){
            return script_error{pre.toString(), error_line(pre, "<preload>"), true};
        }
    }

    QJSValue res = eng_.evaluate(source, "<script>");

    if(res.isError()){
        return script_error{res.toString(), error_line(res, "<script>"), false};
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


