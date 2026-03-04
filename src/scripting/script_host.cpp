#include "scripting/script_host.h"

#include <QDebug>

script_host::script_host(QObject* parent) : QObject(parent){
    bridge_ = new js_bridge(&eng_, &sc_, &eng_);

    eng_.globalObject().setProperty("SceneApi", eng_.newQObject(bridge_));

    //simple print() ???????
    //
    //not very simple now lmaooo
    auto* native = new NativeConsole(&eng_);

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

QString script_host::run(const QString& source, const QString& preload_source){
    sc_ = scene{};  //resets the scene every start

    if(!preload_source.isEmpty()){
        QJSValue pre = eng_.evaluate(preload_source, "<preload>");

        if(pre.isError()){
            return QString("Preload error: %1 (line %2)")
                .arg(pre.toString())
                .arg(pre.property("lineNumber").toInt());
        }
    }

    QJSValue res = eng_.evaluate(source, "<script>");

    if(res.isError()){
        return QString("Error: %1 (line %2)")
            .arg(res.toString())
            .arg(res.property("lineNumber").toInt());
    }

    return {};
}


