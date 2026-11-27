#pragma once

#include "ui/script_editor.h"

class QPlainTextEdit;

// Lightweight fallback editor. Always available; used when Monaco is
// disabled at build time, missing at run time, or fails to start.
class PlainScriptEditor : public ScriptEditor {
    Q_OBJECT

    public:
        explicit PlainScriptEditor(QWidget* parent = nullptr);

        void show_errors(const QList<ScriptError>& errors) override;
        void go_to_line(int line, int column = 1) override;
        QString backend_name() const override;

        QPlainTextEdit* text_edit() const;

    protected:
        void load_into_widget(const QString& text) override;

    private:
        QPlainTextEdit* m_edit = nullptr;
        bool m_loading = false;
};
