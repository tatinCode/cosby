#pragma once

class QString;
class QWidget;
class ScriptEditor;

// Must run before QApplication is constructed (registers Monaco's URL
// scheme). Does nothing in builds without Monaco.
void prepare_script_editor_backend();

// Creates Monaco when it is built in, its files are present and
// COSBY_PLAIN_EDITOR is not set; otherwise the plain editor.
// `notice` receives a reason when Monaco was expected but not used.
ScriptEditor* create_script_editor(QWidget* parent, QString* notice = nullptr);

// Always the plain editor (fallback after a Monaco failure).
ScriptEditor* create_plain_script_editor(QWidget* parent);
