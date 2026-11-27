#pragma once

#include "core/scene.h"
#include "scripting/js_bridge.h"

#include <QObject>
#include <QJSEngine>
#include <QJSValue>

#include <optional>

struct script_error {
    QString message;
    int line = 0;               // 1-based, 0 when unknown
    bool in_preload = false;    // true when the error is in preload_source
};

class script_host : public QObject{
    Q_OBJECT
        
    public:
        explicit script_host(QObject* parent = nullptr);

        // Evaluates the script into a fresh scene; returns the error, if any.
        std::optional<script_error> evaluate(const QString& source, const QString& preload_source = QString());

        // Same as evaluate(), but formats the error as text (empty on success).
        QString run(const QString& source, const QString& preload_source = QString());
        scene& current_scene() {
            return sc_;
        }

    signals:
        // Text from print() / console.* in the script.
        void output(const QString& text);

    private:
        QJSEngine eng_;
        scene sc_;
        js_bridge* bridge_{};
};


