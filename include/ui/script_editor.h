#pragma once

#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

struct ScriptError {
    int line = 0;       // 1-based
    int column = 0;     // 1-based, 0 marks the whole line
    QString message;
};

// The script editor as MainWindow sees it. Implementations: PlainScriptEditor
// (QPlainTextEdit) and MonacoEditor (QWebEngineView, optional at build time).
//
// The base class owns the text/saved-text bookkeeping, so "modified" means
// exactly "differs from what was last loaded or saved", including after undo.
class ScriptEditor : public QWidget {
    Q_OBJECT

    public:
        explicit ScriptEditor(QWidget* parent = nullptr);

        // Latest text known to C++. Call sync() first when it must include
        // the very last keystroke (asynchronous editors lag slightly behind).
        QString text() const;
        QString saved_text() const;
        bool is_modified() const;

        // Replaces the text, clears undo history and marks it as unmodified.
        void set_text(const QString& text);

        // Records that `text` was written to disk.
        void mark_saved(const QString& text);

        // Moves the state of another editor over (used when falling back).
        void restore_state(const QString& text, const QString& saved_text);

        // Calls `done` once text() is current. May be called synchronously.
        virtual void sync(std::function<void()> done);

        // Highlights errors; an empty list clears them.
        virtual void show_errors(const QList<ScriptError>& errors) = 0;
        virtual void go_to_line(int line, int column = 1) = 0;

        // Short name for status messages, e.g. "Monaco".
        virtual QString backend_name() const = 0;

    signals:
        void modified_changed(bool modified);
        void save_requested();
        void run_requested();

        // The editor cannot work any more; MainWindow replaces it.
        void failed(const QString& reason);

    protected:
        // Puts `text` into the widget and resets its undo history.
        virtual void load_into_widget(const QString& text) = 0;

        // Subclasses call this whenever the user changes the text.
        void editor_text_changed(const QString& text);

        // Bumped by set_text()/restore_state(); async results carrying an
        // older generation are stale and must be dropped.
        quint64 text_generation() const;

    private:
        void update_modified();

        QString m_text;
        QString m_saved_text;
        bool m_modified = false;
        quint64 m_generation = 0;
};
