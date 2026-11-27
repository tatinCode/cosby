#pragma once

#include "ui/script_editor.h"

#include <memory>

class QWebChannel;
class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineView;
class MonacoBridge;

// Monaco running in a QWebEngineView. The page is served from the
// cosby-editor:// scheme: host page from Qt resources, Monaco from
// monaco_directory(). The text is pushed to C++ on every change and cached
// in ScriptEditor, so text() stays synchronous; sync() catches up on the
// last keystroke before save/run.
class MonacoEditor : public ScriptEditor {
    Q_OBJECT

    public:
        // Call once, before QApplication exists.
        static void register_url_scheme();

        // Folder containing vs/loader.js. $COSBY_MONACO_DIR overrides the
        // default <executable dir>/monaco.
        static QString monaco_directory();
        static bool is_available(QString* reason = nullptr);

        explicit MonacoEditor(QWidget* parent = nullptr);
        ~MonacoEditor() override;

        bool is_ready() const;

        // For tests: drive the page directly.
        QWebEnginePage* web_page() const;

        void sync(std::function<void()> done) override;
        void show_errors(const QList<ScriptError>& errors) override;
        void go_to_line(int line, int column = 1) override;
        QString backend_name() const override;

    signals:
        void ready();

    protected:
        void load_into_widget(const QString& text) override;

    private:
        void on_page_ready();
        void fail(const QString& reason);
        void push_errors();
        bool is_current_generation(double generation) const;

        QWebEngineProfile* m_profile = nullptr;
        QWebEngineView* m_view = nullptr;
        QWebChannel* m_channel = nullptr;
        MonacoBridge* m_bridge = nullptr;

        QList<ScriptError> m_errors;
        bool m_ready = false;
        bool m_failed = false;

        // Cleared first thing in the destructor so late callbacks do nothing.
        std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
