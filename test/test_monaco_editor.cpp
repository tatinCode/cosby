#include "ui/monaco/monaco_editor.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QWebEnginePage>
#include <QtTest>

#include <memory>

namespace {
    constexpr int ready_timeout_ms = 60000;
    constexpr int js_timeout_ms = 15000;

    QVariant eval(MonacoEditor& editor, const QString& script){
        QVariant result;
        bool done = false;

        editor.web_page()->runJavaScript(script, [&](const QVariant& value){
            result = value;
            done = true;
        });

        if(!QTest::qWaitFor([&]{ return done; }, js_timeout_ms)){
            qWarning() << "JavaScript did not answer:" << script;
        }

        return result;
    }

    QString page_text(MonacoEditor& editor){
        return eval(editor, "window.cosbyGetText().text").toString();
    }

    void type_in_page(MonacoEditor& editor, const QString& text){
        // JSON-quote the text for the script literal.
        const QString literal = QString::fromUtf8(QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact));
        eval(editor, QString("monaco.editor.getEditors()[0].trigger('test', 'type', {text: %1[0]}); true").arg(literal));
    }

    // Asks the TypeScript worker for completions at `offset` in the current model.
    QStringList completions(MonacoEditor& editor, int offset){
        eval(editor, QString(R"JS(
            window.__cosby_test = null;
            (async function () {
                const model = monaco.editor.getEditors()[0].getModel();
                const get_worker = await monaco.languages.typescript.getJavaScriptWorker();
                const worker = await get_worker(model.uri);
                const result = await worker.getCompletionsAtPosition(model.uri.toString(), %1);
                window.__cosby_test = result ? result.entries.map(function (e) { return e.name; }) : [];
            })().catch(function (e) { window.__cosby_test = ['error: ' + e]; });
            true
        )JS").arg(offset));

        QVariant names;
        (void)QTest::qWaitFor([&]{
            names = eval(editor, "window.__cosby_test");
            return names.isValid() && !names.isNull();
        }, js_timeout_ms);

        return names.toStringList();
    }

    bool wait_ready(MonacoEditor& editor){
        if(editor.is_ready()){
            return true;
        }

        QSignalSpy ready(&editor, &MonacoEditor::ready);
        QSignalSpy failed(&editor, &ScriptEditor::failed);

        (void)QTest::qWaitFor([&]{ return !ready.isEmpty() || !failed.isEmpty(); }, ready_timeout_ms);

        if(!failed.isEmpty()){
            qWarning() << "Monaco failed:" << failed.first().first().toString();
        }

        return editor.is_ready();
    }
}

class TestMonacoEditor : public QObject {
    Q_OBJECT

    private:
        std::unique_ptr<MonacoEditor> m_editor;

    private slots:
        void initTestCase(){
            QString reason;

            if(!MonacoEditor::is_available(&reason)){
                QSKIP(qPrintable(reason));
            }

            m_editor = std::make_unique<MonacoEditor>();
            m_editor->resize(800, 600);
            m_editor->show();

            QVERIFY(wait_ready(*m_editor));
        }

        void cleanupTestCase(){
            m_editor.reset();
        }

        void set_text_reaches_page(){
            m_editor->set_text("const a = 1;\nconst b = 2;\n");

            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("const a = 1;\nconst b = 2;\n"), js_timeout_ms);
            QVERIFY(!m_editor->is_modified());
        }

        void special_characters_round_trip(){
            const QString text = "print(\"quote\" + '\\'' + `${1}`);\n// ünïcödé \\ </script>\n";

            m_editor->set_text(text);

            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), text, js_timeout_ms);
        }

        void typing_updates_text_and_modified(){
            m_editor->set_text("a");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("a"), js_timeout_ms);

            QSignalSpy spy(m_editor.get(), &ScriptEditor::modified_changed);

            type_in_page(*m_editor, "b");

            QTRY_COMPARE_WITH_TIMEOUT(m_editor->text(), QString("ba"), js_timeout_ms);
            QVERIFY(m_editor->is_modified());
            QCOMPARE(spy.count(), 1);
        }

        void undo_to_saved_text_is_unmodified(){
            m_editor->set_text("a");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("a"), js_timeout_ms);

            type_in_page(*m_editor, "b");
            QTRY_VERIFY_WITH_TIMEOUT(m_editor->is_modified(), js_timeout_ms);

            eval(*m_editor, "monaco.editor.getEditors()[0].trigger('test', 'undo', null); true");

            QTRY_VERIFY_WITH_TIMEOUT(!m_editor->is_modified(), js_timeout_ms);
            QCOMPARE(m_editor->text(), QString("a"));
        }

        void set_text_starts_fresh_undo_history(){
            m_editor->set_text("first");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("first"), js_timeout_ms);

            m_editor->set_text("second");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("second"), js_timeout_ms);

            eval(*m_editor, "monaco.editor.getEditors()[0].trigger('test', 'undo', null); true");

            QCOMPARE(page_text(*m_editor), QString("second"));
            QVERIFY(!m_editor->is_modified());
        }

        void sync_delivers_latest_text(){
            m_editor->set_text("x");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("x"), js_timeout_ms);

            // Edit and sync without waiting for the change notification.
            m_editor->web_page()->runJavaScript(
                    "monaco.editor.getEditors()[0].trigger('test', 'type', {text: 'y'})");

            bool synced = false;
            m_editor->sync([&]{ synced = true; });

            QTRY_VERIFY_WITH_TIMEOUT(synced, js_timeout_ms);
            QCOMPARE(m_editor->text(), QString("yx"));
        }

        void mark_saved_clears_modified(){
            m_editor->set_text("s");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("s"), js_timeout_ms);

            type_in_page(*m_editor, "t");
            QTRY_VERIFY_WITH_TIMEOUT(m_editor->is_modified(), js_timeout_ms);

            m_editor->mark_saved(m_editor->text());
            QVERIFY(!m_editor->is_modified());
        }

        void errors_become_markers(){
            m_editor->set_text("one\ntwo\nthree\n");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("one\ntwo\nthree\n"), js_timeout_ms);

            m_editor->show_errors({{2, 0, "ReferenceError: x is not defined"}});

            QTRY_COMPARE_WITH_TIMEOUT(
                    eval(*m_editor, "monaco.editor.getModelMarkers({owner: 'cosby'}).map(m => m.startLineNumber + ':' + m.message).join('|')").toString(),
                    QString("2:ReferenceError: x is not defined"),
                    js_timeout_ms);

            m_editor->show_errors({});

            QTRY_COMPARE_WITH_TIMEOUT(
                    eval(*m_editor, "monaco.editor.getModelMarkers({owner: 'cosby'}).length").toInt(),
                    0,
                    js_timeout_ms);
        }

        void warnings_become_yellow_markers(){
            m_editor->set_text("one\ntwo\nthree\n");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("one\ntwo\nthree\n"), js_timeout_ms);

            m_editor->show_errors({{1, 0, "broken"}, {3, 0, "a.png: image not found", true}});

            QTRY_COMPARE_WITH_TIMEOUT(
                    eval(*m_editor, "monaco.editor.getModelMarkers({owner: 'cosby'})"
                                    ".map(m => m.startLineNumber + ':' + (m.severity === monaco.MarkerSeverity.Warning"
                                    " ? 'warning' : m.severity === monaco.MarkerSeverity.Error ? 'error' : '?')).join('|')").toString(),
                    QString("1:error|3:warning"),
                    js_timeout_ms);

            m_editor->show_errors({});
        }

        void go_to_line_moves_cursor(){
            m_editor->set_text("one\ntwo\nthree\n");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("one\ntwo\nthree\n"), js_timeout_ms);

            m_editor->go_to_line(3, 2);

            QTRY_COMPARE_WITH_TIMEOUT(
                    eval(*m_editor, "(p => p.lineNumber + ':' + p.column)(monaco.editor.getEditors()[0].getPosition())").toString(),
                    QString("3:2"),
                    js_timeout_ms);
        }

        void autocomplete_knows_cosby_api(){
            m_editor->set_text("SceneApi.");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString("SceneApi."), js_timeout_ms);

            const QStringList names = completions(*m_editor, 9);

            QVERIFY2(names.contains("sprite"), qPrintable(names.join(", ")));
            QVERIFY2(names.contains("add_sprite"), qPrintable(names.join(", ")));
        }

        void autocomplete_has_no_dom(){
            m_editor->set_text("");
            QTRY_COMPARE_WITH_TIMEOUT(page_text(*m_editor), QString(), js_timeout_ms);

            const QStringList names = completions(*m_editor, 0);

            QVERIFY2(names.contains("SceneApi"), qPrintable(names.mid(0, 40).join(", ")));
            QVERIFY(names.contains("print"));
            QVERIFY(names.contains("Math"));
            QVERIFY2(!names.contains("document"), "DOM types should not be offered");
        }

        void text_set_before_ready_is_shown_and_tracked(){
            MonacoEditor early;
            early.set_text("early");

            QVERIFY(wait_ready(early));
            QTRY_COMPARE_WITH_TIMEOUT(page_text(early), QString("early"), js_timeout_ms);
            QVERIFY(!early.is_modified());

            // Changes after the initial push must be accepted (generation matches).
            type_in_page(early, "!");
            QTRY_COMPARE_WITH_TIMEOUT(early.text(), QString("!early"), js_timeout_ms);
            QVERIFY(early.is_modified());
        }
};

int main(int argc, char* argv[]){
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    MonacoEditor::register_url_scheme();

    QApplication app(argc, argv);
    TestMonacoEditor test;

    return QTest::qExec(&test, argc, argv);
}

#include "test_monaco_editor.moc"
