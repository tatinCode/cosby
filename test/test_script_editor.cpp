#include "ui/plain_script_editor.h"

#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTextCursor>
#include <QtTest>

class TestScriptEditor : public QObject {
    Q_OBJECT

    private:
        static void type(PlainScriptEditor& editor, const QString& text){
            QTextCursor cursor = editor.text_edit()->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.insertText(text);
        }

    private slots:
        void starts_empty_and_unmodified(){
            PlainScriptEditor editor;

            QCOMPARE(editor.text(), QString());
            QVERIFY(!editor.is_modified());
        }

        void set_text_is_unmodified_and_clears_undo(){
            PlainScriptEditor editor;
            QSignalSpy spy(&editor, &ScriptEditor::modified_changed);

            editor.set_text("let a = 1;\n");

            QCOMPARE(editor.text(), QString("let a = 1;\n"));
            QCOMPARE(editor.saved_text(), QString("let a = 1;\n"));
            QVERIFY(!editor.is_modified());
            QCOMPARE(spy.count(), 0);
            QVERIFY(!editor.text_edit()->document()->isUndoAvailable());
        }

        void typing_marks_modified_once(){
            PlainScriptEditor editor;
            editor.set_text("a");

            QSignalSpy spy(&editor, &ScriptEditor::modified_changed);

            type(editor, "b");
            type(editor, "c");

            QCOMPARE(editor.text(), QString("abc"));
            QVERIFY(editor.is_modified());
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.at(0).at(0).toBool(), true);
        }

        void undo_back_to_saved_text_is_unmodified(){
            PlainScriptEditor editor;
            editor.set_text("a");

            type(editor, "b");
            QVERIFY(editor.is_modified());

            editor.text_edit()->undo();

            QCOMPARE(editor.text(), QString("a"));
            QVERIFY(!editor.is_modified());
        }

        void mark_saved_clears_modified(){
            PlainScriptEditor editor;
            editor.set_text("a");
            type(editor, "b");

            QSignalSpy spy(&editor, &ScriptEditor::modified_changed);

            editor.mark_saved(editor.text());

            QVERIFY(!editor.is_modified());
            QCOMPARE(editor.saved_text(), QString("ab"));
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.at(0).at(0).toBool(), false);
        }

        void mark_saved_with_older_text_stays_modified(){
            // Text changed after it was written: still unsaved.
            PlainScriptEditor editor;
            editor.set_text("a");
            type(editor, "b");

            const QString written = editor.text();
            type(editor, "c");

            editor.mark_saved(written);

            QVERIFY(editor.is_modified());
        }

        void set_text_over_modified_resets(){
            PlainScriptEditor editor;
            editor.set_text("a");
            type(editor, "b");

            QSignalSpy spy(&editor, &ScriptEditor::modified_changed);

            editor.set_text("other project");

            QVERIFY(!editor.is_modified());
            QCOMPARE(editor.text(), QString("other project"));
            QCOMPARE(spy.count(), 1);
        }

        void restore_state_keeps_unsaved_changes(){
            PlainScriptEditor editor;

            editor.restore_state("edited", "on disk");

            QCOMPARE(editor.text(), QString("edited"));
            QCOMPARE(editor.saved_text(), QString("on disk"));
            QCOMPARE(editor.text_edit()->toPlainText(), QString("edited"));
            QVERIFY(editor.is_modified());

            editor.text_edit()->setPlainText("on disk");
            QVERIFY(!editor.is_modified());
        }

        void sync_calls_back_immediately(){
            PlainScriptEditor editor;
            bool called = false;

            editor.sync([&called]{ called = true; });

            QVERIFY(called);
        }

        void show_errors_highlights_valid_lines_only(){
            PlainScriptEditor editor;
            editor.set_text("one\ntwo\nthree\n");

            editor.show_errors({{2, 0, "bad"}, {99, 0, "out of range"}});
            QCOMPARE(editor.text_edit()->extraSelections().size(), 1);
            QCOMPARE(editor.text_edit()->extraSelections().first().cursor.blockNumber(), 1);

            editor.show_errors({});
            QVERIFY(editor.text_edit()->extraSelections().isEmpty());
        }

        void go_to_line_moves_cursor(){
            PlainScriptEditor editor;
            editor.set_text("one\ntwo\nthree\n");

            editor.go_to_line(3, 2);

            QCOMPARE(editor.text_edit()->textCursor().blockNumber(), 2);
            QCOMPARE(editor.text_edit()->textCursor().positionInBlock(), 1);

            editor.go_to_line(0);   // invalid: ignored
            QCOMPARE(editor.text_edit()->textCursor().blockNumber(), 2);
        }
};

QTEST_MAIN(TestScriptEditor)
#include "test_script_editor.moc"
