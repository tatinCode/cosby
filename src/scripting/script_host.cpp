#include "scripting/script_host.h"

#include <QDebug>

script_host::script_host(QObject* parent) : QObject(parent){
    bridge_ = new js_bridge(&eng_, &sc_, &eng_);

    eng_.globalObject().setProperty("SceneApi", eng_.newQObject(bridge_));

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
            )JS"),
            QStringLiteral("<bootstrap>")   //filename for error stacks
            );
}

std::optional<script_error> script_host::evaluate(const QString& source, const QString& preload_source){
    sc_ = scene{};  //resets the scene every start

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


