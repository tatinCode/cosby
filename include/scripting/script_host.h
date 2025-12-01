#pragma once

#include "core/scene.h"
#include "scripting/js_bridge.h"

#include <QObject>
#include <QJSEngine>
#include <QJSValue>

class script_host : public QObject{
    Q_OBJECT
        
    public:
        explicit script_host(QObject* parent = nullptr);

        QString run(const QString& source, const QString& preload_source = QString());
        scene& current_scene() {
            return sc_;
        }

    private:
        QJSEngine eng_;
        scene sc_;
        js_bridge* bridge_{};
};


