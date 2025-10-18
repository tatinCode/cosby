#pragma once

#include <QObject>
#include <QJSEngine>

class ScriptHost : public QObject{
    Q_OBJECT

public:
    explicit ScriptHost(QObject* parent = nullptr);
    QString run(const QString& code);

      // TODO: expose scene graph getters
      //
private:
    QJSEngine m_js;
};
