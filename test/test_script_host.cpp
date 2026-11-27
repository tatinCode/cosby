#include "scripting/script_host.h"
#include "cosby_test.h"

#include <QSignalSpy>
#include <QtTest>

class TestScriptHost : public QObject {
    Q_OBJECT

    private slots:
        void records_the_creating_line(){
            script_host host;

            const auto error = host.evaluate(
                    "// comment\n"
                    "SceneApi.sprite('a.png', {});\n"
                    "\n"
                    "SceneApi.add_sprite('b.png', 'Background', 'Center', 0, 0);\n");

            QVERIFY2(!error.has_value(), error ? qPrintable(error->message) : "");

            const auto& sprites = host.current_scene().sprites;
            QCOMPARE(sprites.size(), qsizetype(2));
            QCOMPARE(sprites.at(0).source_line, 2);
            QCOMPARE(sprites.at(1).source_line, 4);
            QCOMPARE(sprites.at(1).layer, layer_t::background);
        }

        void loops_share_one_line(){
            script_host host;

            QVERIFY(!host.evaluate(
                    "for (let i = 0; i < 5; ++i) {\n"
                    "    SceneApi.sprite('p.png', { x: i });\n"
                    "}\n").has_value());

            const auto& sprites = host.current_scene().sprites;
            QCOMPARE(sprites.size(), qsizetype(5));

            for(const sprite& s : sprites){
                QCOMPARE(s.source_line, 2);
            }
            QCOMPARE(sprites.at(4).x, 4.0);
        }

        void helper_functions_use_the_line_inside_the_helper(){
            script_host host;

            QVERIFY(!host.evaluate(
                    "function star(x) {\n"
                    "    return SceneApi.sprite('star.png', { x: x });\n"
                    "}\n"
                    "star(1);\n"
                    "star(2);\n"
                    "[3].map(x => star(x));\n").has_value());

            for(const sprite& s : host.current_scene().sprites){
                QCOMPARE(s.source_line, 2);
            }
        }

        void preload_sprites_have_no_line(){
            script_host host;

            QVERIFY(!host.evaluate("", "SceneApi.sprite('pre.png');").has_value());

            QCOMPARE(host.current_scene().sprites.size(), qsizetype(1));
            QCOMPARE(host.current_scene().sprites.first().source_line, 0);
        }

        void name_option_is_stored(){
            script_host host;

            QVERIFY(!host.evaluate("SceneApi.sprite('a.png', { name: 'flash' });").has_value());
            QCOMPARE(host.current_scene().sprites.first().name, QString("flash"));
        }

        void sprite_without_options_works(){
            script_host host;

            QVERIFY(!host.evaluate("SceneApi.sprite('a.png').fade({ startTime: 0, endTime: 10, startValue: 0, endValue: 1 });").has_value());

            const sprite& s = host.current_scene().sprites.first();
            QCOMPARE(s.layer, layer_t::foreground);
            QCOMPARE(s.x, 320.0);
            QCOMPARE(s.cmds.size(), qsizetype(1));
        }

        void earlier_handles_stay_valid_after_many_adds(){
            // Regression: proxies used to hold a sprite* into the QVector, which
            // dangled as soon as a later add reallocated it.
            script_host host;

            const auto error = host.evaluate(
                    "const first = SceneApi.sprite('first.png', {});\n"
                    "for (let i = 0; i < 1000; ++i) SceneApi.sprite('filler.png', {});\n"
                    "first.fade({ startTime: 100, endTime: 200, startValue: 0, endValue: 1 })\n"
                    "     .moveX({ startTime: 0, endTime: 1, endValue: 5 });\n");

            QVERIFY2(!error.has_value(), error ? qPrintable(error->message) : "");

            const auto& sprites = host.current_scene().sprites;
            QCOMPARE(sprites.size(), qsizetype(1001));

            const sprite& first = sprites.first();
            QCOMPARE(first.path, QString("first.png"));
            QCOMPARE(first.cmds.size(), qsizetype(2));
            QCOMPARE(first.cmds.at(0).t0, 100);
            QCOMPARE(first.cmds.at(1).v0, 320.0);    // moveX defaults to the sprite's x

            for(qsizetype i = 1; i < sprites.size(); ++i){
                QVERIFY(sprites.at(i).cmds.isEmpty());
            }
        }

        void handles_from_an_earlier_run_fail_loudly(){
            script_host host;

            QVERIFY(!host.evaluate("var kept = SceneApi.sprite('old.png', {});").has_value());

            const auto error = host.evaluate(
                    "SceneApi.sprite('new.png', {});\n"
                    "kept.fade({ startTime: 0, endTime: 1, startValue: 0, endValue: 1 });\n");

            QVERIFY(error.has_value());
            QVERIFY2(error->message.contains("earlier run"), qPrintable(error->message));
            QCOMPARE(error->line, 2);

            // Must not have edited the new run's sprite at the same index.
            QCOMPARE(host.current_scene().sprites.size(), qsizetype(1));
            QVERIFY(host.current_scene().sprites.first().cmds.isEmpty());
        }

        void each_run_starts_with_an_empty_scene(){
            script_host host;

            QVERIFY(!host.evaluate("SceneApi.sprite('a.png', {});").has_value());
            QVERIFY(!host.evaluate("SceneApi.sprite('b.png', {});").has_value());

            QCOMPARE(host.current_scene().sprites.size(), qsizetype(1));
            QCOMPARE(host.current_scene().sprites.first().path, QString("b.png"));
        }

        void print_reaches_the_output_signal(){
            script_host host;
            QSignalSpy spy(&host, &script_host::output);

            QVERIFY(!host.evaluate("print('a', 1); console.log('b');").has_value());

            QCOMPARE(spy.count(), 2);
            QCOMPARE(spy.at(0).at(0).toString(), QString("a 1"));
            QCOMPARE(spy.at(1).at(0).toString(), QString("b"));
        }

        void error_line_is_reported(){
            script_host host;

            const auto error = host.evaluate("let a = 1;\nnot_defined();\n");

            QVERIFY(error.has_value());
            QCOMPARE(error->line, 2);
            QVERIFY(!error->in_preload);
        }
};

COSBY_TEST(TestScriptHost)
#include "test_script_host.moc"
