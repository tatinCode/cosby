#include "ui/script_editor_factory.h"
#include "ui/plain_script_editor.h"

#include <QString>

#if COSBY_HAS_MONACO
#include "ui/monaco/monaco_editor.h"

#include <QCoreApplication>
#endif

void prepare_script_editor_backend(){
#if COSBY_HAS_MONACO
    // Qt WebEngine shares GL contexts with the widget stack; Qt asks for this
    // attribute to be set before the application object exists.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    MonacoEditor::register_url_scheme();
#endif
}

ScriptEditor* create_script_editor(QWidget* parent, QString* notice){
    if(notice){
        notice->clear();
    }

#if COSBY_HAS_MONACO
    if(qEnvironmentVariableIntValue("COSBY_PLAIN_EDITOR") != 0){
        return create_plain_script_editor(parent);
    }

    QString reason;

    if(MonacoEditor::is_available(&reason)){
        return new MonacoEditor(parent);
    }

    if(notice){
        *notice = reason;
    }
#endif

    return create_plain_script_editor(parent);
}

ScriptEditor* create_plain_script_editor(QWidget* parent){
    return new PlainScriptEditor(parent);
}
