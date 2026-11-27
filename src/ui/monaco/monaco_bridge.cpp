#include "ui/monaco/monaco_bridge.h"

MonacoBridge::MonacoBridge(const QString& type_definitions, QObject* parent) :
    QObject(parent),
    m_type_definitions(type_definitions)
{
}

QString MonacoBridge::type_definitions() const{
    return m_type_definitions;
}

void MonacoBridge::notify_ready(){
    emit ready();
}

void MonacoBridge::notify_content_changed(const QString& text, double generation){
    emit content_changed(text, generation);
}

void MonacoBridge::notify_save(){
    emit save();
}

void MonacoBridge::notify_run(){
    emit run();
}

void MonacoBridge::notify_failure(const QString& reason){
    emit failure(reason);
}
