#include "ui/script_editor.h"

ScriptEditor::ScriptEditor(QWidget* parent) : QWidget(parent){
}

QString ScriptEditor::text() const{
    return m_text;
}

QString ScriptEditor::saved_text() const{
    return m_saved_text;
}

bool ScriptEditor::is_modified() const{
    return m_modified;
}

void ScriptEditor::set_text(const QString& text){
    restore_state(text, text);
}

void ScriptEditor::mark_saved(const QString& text){
    m_saved_text = text;
    update_modified();
}

void ScriptEditor::restore_state(const QString& text, const QString& saved_text){
    ++m_generation;

    m_text = text;
    m_saved_text = saved_text;

    load_into_widget(text);

    // load_into_widget() may report the text back; m_text is already equal then.
    m_text = text;
    update_modified();
}

void ScriptEditor::sync(std::function<void()> done){
    if(done){
        done();
    }
}

void ScriptEditor::editor_text_changed(const QString& text){
    m_text = text;
    update_modified();
}

quint64 ScriptEditor::text_generation() const{
    return m_generation;
}

void ScriptEditor::update_modified(){
    const bool modified = m_text != m_saved_text;

    if(modified == m_modified){
        return;
    }

    m_modified = modified;
    emit modified_changed(m_modified);
}
