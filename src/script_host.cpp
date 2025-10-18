#include "script_host.h"

#include <QJSValue>
#include <QJSValueList>

ScriptHost::ScriptHost(QObject* parent) : QObject(parent){
    //expose a minimal api surface later via QObject wrappers
    //exmaple:
    //QObject* api = ...;
    //m_js.globalObject().setProperty("api", m_js.newQObject(api));
}

QString ScriptHost::run(const QString& code){
    auto v = m_js.evaluate(code, "script.js");

    if(v.isError()){
        return QString("JS Error, %1 at line %2")
            .arg(v.toString())
            .arg(v.property("line_number").toInt());
    }
}
