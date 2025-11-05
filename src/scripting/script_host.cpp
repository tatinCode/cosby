#include "scripting/script_host.h"

#include <QDebug>

script_host::script_host(QObject* parent) : QObject(parent){
    bridge_ = new js_bridge(&eng_, &sc_, &eng_);

    eng_.globalObject().setProperty("SceneApi", eng_.newQObject(bridge_));

    //simple print() ???????
    //
    auto print_fn = eng_.newFunction([](QJSContext*, QJSValueList args){
            QString out;

            for(auto& v : args){
                out += v.toString() + " ";
            }

            qInfo().noquote() << out.trimmed();

            return QJSValue();
            });

    eng_.globalObject().setProperty("print", print_fn);
}

QString script_host::run(const QString& source){
    sc_ = scene{};

    QJSValue res = eng_.evaluate(source, "<user_script>");

    if(res.isError()){
        return QString("Error: %1 (line %2))
            .arg(res.toString())
            .arg(res.property("lineNumber").toInt());
    }

    return {};
}

