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

        void accepts_all_easings_by_name_or_number(){
            script_host host;

            const auto error = host.evaluate(
                    "SceneApi.sprite('a.png')\n"
                    "    .fade({ startTime: 0, endTime: 10, startValue: 0, endValue: 1, easing: 'bounceout' })\n"
                    "    .scale('ElasticHalfOut', 0, 10, 1, 2)\n"
                    "    .moveX({ startTime: 0, endTime: 10, endValue: 5, easing: 24 })\n"
                    "    .rotate({ startTime: 0, endTime: 10, endValue: 1, easing: 'Out' })\n"
                    "    .color({ startTime: 0, endTime: 10, startValue: [0, 0, 0] });\n");

            QVERIFY2(!error.has_value(), error ? qPrintable(error->message) : "");

            const auto& cmds = host.current_scene().sprites.first().cmds;
            QCOMPARE(cmds.at(0).easing, easing_t::bounce_out);
            QCOMPARE(cmds.at(1).easing, easing_t::elastic_half_out);
            QCOMPARE(cmds.at(2).easing, easing_t::elastic_in);
            QCOMPARE(cmds.at(3).easing, easing_t::out);
            QCOMPARE(cmds.at(4).easing, easing_t::linear);
        }

        void unknown_easings_stop_the_script_at_their_line(){
            for(const char* call : {
                    "s.fade({ startTime: 0, endTime: 1, easing: 'Wobbly' });",
                    "s.fade('Wobbly', 0, 1, 0, 1);",
                    "s.moveY({ startTime: 0, endTime: 1, easing: 35 });",
                    "s.scaleVec({ startTime: 0, endTime: 1, easing: 'Quad' });",
                    "s.color({ startTime: 0, endTime: 1, easing: '' });"}){
                script_host host;
                const auto error = host.evaluate(QString("const s = SceneApi.sprite('a.png');\n%1\n").arg(call));

                QVERIFY2(error.has_value(), call);
                QVERIFY2(error->message.contains("Unknown easing"), qPrintable(error->message));
                QCOMPARE(error->line, 2);
                QVERIFY(host.current_scene().sprites.first().cmds.isEmpty());
            }
        }

        void loops_collect_their_own_commands(){
            script_host host;

            const auto error = host.evaluate(
                    "SceneApi.sprite('a.png')\n"
                    "    .fade({ startTime: 0, endTime: 100, startValue: 0, endValue: 1 })\n"
                    "    .loop(1000, 4, l => {\n"
                    "        l.scale({ startTime: 0, endTime: 250, startValue: 1, endValue: 2 })\n"
                    "         .scale({ startTime: 250, endTime: 500, startValue: 2, endValue: 1 });\n"
                    "        SceneApi.sprite('other.png');\n"       // may move the sprite in memory
                    "    })\n"
                    "    .rotate({ startTime: 0, endTime: 1, endValue: 1 });\n");

            QVERIFY2(!error.has_value(), error ? qPrintable(error->message) : "");

            const sprite& s = host.current_scene().sprites.first();
            QCOMPARE(s.cmds.size(), qsizetype(2));
            QCOMPARE(s.loops.size(), qsizetype(1));
            QCOMPARE(s.loops[0].start, 1000);
            QCOMPARE(s.loops[0].count, 4);
            QCOMPARE(s.loops[0].cmds.size(), qsizetype(2));
            QCOMPARE(s.loops[0].cmds[1].t0, 250);
        }

        void triggers_collect_their_own_commands(){
            script_host host;

            const auto error = host.evaluate(
                    "SceneApi.sprite('a.png')\n"
                    "    .trigger('HitSoundSoftClap', 0, 5000, t => t.fade({ startTime: 0, endTime: 50, startValue: 1, endValue: 0 }))\n"
                    "    .trigger('Passing', 100, 200, t => t.scale('Linear', 0, 1, 1, 2), 3);\n");

            QVERIFY2(!error.has_value(), error ? qPrintable(error->message) : "");

            const sprite& s = host.current_scene().sprites.first();
            QVERIFY(s.cmds.isEmpty());
            QCOMPARE(s.triggers.size(), qsizetype(2));
            QCOMPARE(s.triggers[0].trigger, QString("HitSoundSoftClap"));
            QCOMPARE(s.triggers[0].end, 5000);
            QCOMPARE(s.triggers[0].group, 0);
            QCOMPARE(s.triggers[0].cmds.size(), qsizetype(1));
            QCOMPARE(s.triggers[1].group, 3);
            QCOMPARE(s.triggers[1].cmds.first().type, cmd_t::scale);
        }

        void bad_loops_and_triggers_are_errors(){
            const std::pair<const char*, const char*> cases[] = {
                {"s.loop(0, 2, l => l.loop(0, 2, () => {}));", "can't contain another loop"},
                {"s.trigger('Passing', 0, 1, t => t.trigger('Failing', 0, 1, () => {}));", "can't contain another trigger"},
                {"s.loop(0, 0, l => {});", "at least 1"},
                {"s.loop(0, 2, 'not a function');", "must be a function"},
                {"s.trigger('Clap', 0, 1, t => {});", "Unknown trigger"},
                {"s.trigger('HitSoundClapWhistle', 0, 1, t => {});", "Unknown trigger"},
                {"s.trigger('Passing', 10, 5, t => {});", "before start"},
                {"s.trigger('Passing', 0, 5, t => {}, -1);", "group"},
                {"s.loop(0, 2, l => { throw new Error('inner problem'); });", "inner problem"},
            };

            for(const auto& [call, expected] : cases){
                script_host host;
                const auto error = host.evaluate(QString("const s = SceneApi.sprite('a.png');\n%1\n").arg(call));

                QVERIFY2(error.has_value(), call);
                QVERIFY2(error->message.contains(expected), qPrintable(call + QString(" -> ") + error->message));
                QCOMPARE(error->line, 2);
            }
        }

        void animation_records_its_frames(){
            script_host host;

            const auto error = host.evaluate(
                    "\n"
                    "SceneApi.animation('sb/flame.png', { frameCount: 8, frameDelay: 40, loopType: 'loopOnce', layer: 'Background', x: 10 })\n"
                    "    .fade({ startTime: 0, endTime: 1, startValue: 1, endValue: 1 });\n"
                    "SceneApi.animation('sb/b.png', { frameCount: 2, frameDelay: 12.5 });\n");

            QVERIFY2(!error.has_value(), error ? qPrintable(error->message) : "");

            const auto& sprites = host.current_scene().sprites;
            QCOMPARE(sprites[0].frame_count, 8);
            QCOMPARE(sprites[0].frame_delay, 40.0);
            QCOMPARE(sprites[0].loop_type, animation_loop_t::loop_once);
            QCOMPARE(sprites[0].layer, layer_t::background);
            QCOMPARE(sprites[0].x, 10.0);
            QCOMPARE(sprites[0].source_line, 2);
            QCOMPARE(sprites[0].cmds.size(), qsizetype(1));
            QCOMPARE(sprites[1].loop_type, animation_loop_t::loop_forever);
            QCOMPARE(sprites[1].frame_delay, 12.5);
        }

        void bad_animations_are_errors(){
            for(const char* call : {
                    "SceneApi.animation('a.png', { frameDelay: 10 });",
                    "SceneApi.animation('a.png', { frameCount: 0, frameDelay: 10 });",
                    "SceneApi.animation('a.png', { frameCount: 2.5, frameDelay: 10 });",
                    "SceneApi.animation('a.png', { frameCount: 2 });",
                    "SceneApi.animation('a.png', { frameCount: 2, frameDelay: 0 });",
                    "SceneApi.animation('a.png', { frameCount: 2, frameDelay: 10, loopType: 'Bounce' });",
                    "SceneApi.animation('a.png');"}){
                script_host host;
                const auto error = host.evaluate(QString("\n%1\n").arg(call));

                QVERIFY2(error.has_value(), call);
                QCOMPARE(error->line, 2);
                QVERIFY2(host.current_scene().sprites.isEmpty(), call);
            }
        }

        void samples_are_recorded(){
            script_host host;

            const auto error = host.evaluate(
                    "SceneApi.sample(1500, 'sb/hit.wav');\n"
                    "SceneApi.sample(2000, 'sb/fail.ogg', { layer: 'fail', volume: 40 });\n");

            QVERIFY2(!error.has_value(), error ? qPrintable(error->message) : "");

            const auto& samples = host.current_scene().samples;
            QCOMPARE(samples.size(), qsizetype(2));
            QCOMPARE(samples[0].time, 1500);
            QCOMPARE(samples[0].path, QString("sb/hit.wav"));
            QCOMPARE(samples[0].layer, layer_t::foreground);
            QCOMPARE(samples[0].volume, 100);
            QCOMPARE(samples[0].source_line, 1);
            QCOMPARE(samples[1].layer, layer_t::fail);
            QCOMPARE(samples[1].volume, 40);
            QCOMPARE(samples[1].source_line, 2);

            // Cleared with the rest of the scene on the next run.
            QVERIFY(!host.evaluate("").has_value());
            QVERIFY(host.current_scene().samples.isEmpty());
        }

        void bad_samples_are_errors(){
            for(const char* call : {
                    "SceneApi.sample(0, 'a.wav', { layer: 'Overlay' });",
                    "SceneApi.sample(0, 'a.wav', { volume: 101 });",
                    "SceneApi.sample(0, 'a.wav', { volume: -1 });",
                    "SceneApi.sample(0, 'a.wav', { volume: 'loud' });"}){
                script_host host;
                const auto error = host.evaluate(QString("\n%1\n").arg(call));

                QVERIFY2(error.has_value(), call);
                QCOMPARE(error->line, 2);
                QVERIFY(host.current_scene().samples.isEmpty());
            }
        }

        void mistakes_name_the_problem(){
            const struct { const char* call; const char* message; } cases[]{
                {"SceneApi.sprite('a.png', { layer: 'Foregroud' });",
                 "TypeError: Unknown layer \"Foregroud\". Did you mean \"Foreground\"? "
                 "Use one of: Background, Fail, Pass, Foreground, Overlay"},
                {"SceneApi.sprite('a.png', { layer: 'Top' });",
                 "TypeError: Unknown layer \"Top\". Use one of: Background, Fail, Pass, Foreground, Overlay"},
                {"SceneApi.add_sprite('a.png', 'Foreground', 'TopCentre', 0, 0);",
                 "TypeError: Unknown origin \"TopCentre\". Did you mean \"TopCenter\"? Use one of: TopLeft, "
                 "TopCenter, TopRight, CenterLeft, Center, CenterRight, BottomLeft, BottomCenter, BottomRight"},
                {"SceneApi.sprite('a.png').fade({ startTim: 0 });",
                 "TypeError: fade(options): unknown option \"startTim\". Did you mean \"startTime\"?"},
                {"SceneApi.sprite('a.png').flipH({ start: 0 });",
                 "TypeError: flipH(options): unknown option \"start\". Use: startTime, endTime"},
                {"SceneApi.sprite('a.png').fade({ easing: 'QuadOutt' });",
                 "TypeError: Unknown easing \"QuadOutt\". Did you mean \"QuadOut\"?"},
                {"SceneApi.sprite('a.png').move({ startTime: '1000' });",
                 "TypeError: move(options): startTime must be a number (got \"1000\")"},
                {"SceneApi.sprite('a.png').scale({ endValue: undefined });",
                 "TypeError: scale(options): endValue must be a number (got undefined)"},
                {"SceneApi.sprite('a.png', { position: 320 });",
                 "TypeError: sprite(path, options): position must be [x, y] (got 320)"},
                {"SceneApi.sprite('a.png').color({ startValue: [255, 0, 'x'] });",
                 "TypeError: color(options): startValue must be [r, g, b] of numbers (item 2 is \"x\")"},
                {"SceneApi.sprite('a.png').rotate({ startTime: 1000, endTime: 500 });",
                 "RangeError: rotate(): endTime (500) is before startTime (1000)"},
                {"SceneApi.sprite('a.png').fade('Linear', 0, 1000);",
                 "TypeError: fade(easing, startTime, endTime, startValue, endValue): "
                 "startValue must be a number (got undefined)"},
                {"SceneApi.sprite('a.png').fade();",
                 "TypeError: fade(options): the options must be an object like { easing: ... } (got undefined)"},
                {"SceneApi.sprite('a.png').loop('0', 2, l => {});",
                 "TypeError: loop(start, count, body): start must be a number (got \"0\")"},
                {"SceneApi.sample(1000, 'a.wav', { layer: 'Pas' });",
                 "TypeError: Unknown layer \"Pas\". Did you mean \"Pass\"? "
                 "Use one of: Background, Fail, Pass, Foreground, Overlay"},
            };

            for(const auto& c : cases){
                script_host host;
                const auto error = host.evaluate(QString("\n%1\n").arg(c.call));

                QVERIFY2(error.has_value(), c.call);
                QCOMPARE(error->message, QString(c.message));
                QCOMPARE(error->line, 2);
            }
        }

        void mistakes_stop_the_script_at_their_line(){
            for(const char* call : {
                    // names
                    "SceneApi.sprite('a.png', { origin: 'Centr' });",
                    "SceneApi.sprite('a.png', { layer: '' });",
                    "SceneApi.animation('a.png', { frameCount: 2, frameDelay: 10, layer: 'Foregroud' });",
                    "SceneApi.sample(0, 'a.wav', { layer: 'Foregroud' });",
                    "SceneApi.sprite('a.png').trigger('Pasing', 0, 1000, t => {});",
                    // the sprite's own arguments
                    "SceneApi.sprite('a.png', { positon: [0, 0] });",
                    "SceneApi.sprite('a.png', { x: '320' });",
                    "SceneApi.sprite('a.png', { name: 5 });",
                    "SceneApi.sprite('a.png', { layer: 0 });",
                    "SceneApi.sprite('a.png', 'Foreground');",
                    "SceneApi.sprite('a.png', null);",
                    "SceneApi.sprite();",
                    "SceneApi.sprite(undefined, {});",
                    "SceneApi.add_sprite('a.png', 'Foreground', 'Center', 0);",
                    "SceneApi.add_sprite('a.png', 'Foreground', 'Center', '0', 0);",
                    "SceneApi.animation('a.png', { frameCount: 2, frameDelay: 10, frames: 3 });",
                    "SceneApi.sample(undefined, 'a.wav');",
                    "SceneApi.sample(0, 'a.wav', { volum: 50 });",
                    // command options
                    "SceneApi.sprite('a.png').fade({ startTime: 0, endTim: 1000 });",
                    "SceneApi.sprite('a.png').fade({ startTime: NaN });",
                    "SceneApi.sprite('a.png').fade({ startTime: Infinity });",
                    "SceneApi.sprite('a.png').fade({ startTime: 1e12 });",
                    "SceneApi.sprite('a.png').fade({ easing: null });",
                    "SceneApi.sprite('a.png').fade([0, 1000]);",
                    "SceneApi.sprite('a.png').fade(x => x);",
                    "SceneApi.sprite('a.png').move({ startPosition: [0] });",
                    "SceneApi.sprite('a.png').move({ endX: 'left' });",
                    "SceneApi.sprite('a.png').moveX({ endValue: [1, 2] });",
                    "SceneApi.sprite('a.png').moveY();",
                    "SceneApi.sprite('a.png').scaleVec({ startValue: 2 });",
                    "SceneApi.sprite('a.png').scaleVec({ startValue: [1, 2, 3] });",
                    "SceneApi.sprite('a.png').color({ startValue: [255, 0] });",
                    "SceneApi.sprite('a.png').color({ r: '255' });",
                    "SceneApi.sprite('a.png').flipV({ startTime: false });",
                    "SceneApi.sprite('a.png').additive(1000);",
                    // end before start
                    "SceneApi.sprite('a.png').fade({ startTime: 1000, endTime: 0 });",
                    "SceneApi.sprite('a.png').move({ startTime: 1000, endTime: 999 });",
                    "SceneApi.sprite('a.png').scaleVec({ startTime: 10, endTime: 5 });",
                    "SceneApi.sprite('a.png').color('Linear', 1000, 0, 255, 0, 0);",
                    "SceneApi.sprite('a.png').loop(0, 2, l => l.fade({ startTime: 500, endTime: 100 }));",
                    // positional forms
                    "SceneApi.sprite('a.png').fade('Linear', '0', 1000, 0, 1);",
                    "SceneApi.sprite('a.png').scale('Linear', 0, 1000, 1);",
                    "SceneApi.sprite('a.png').rotate(0);",
                    "SceneApi.sprite('a.png').move('Linear', 0, 1000, 0, 0, 100);",
                    "SceneApi.sprite('a.png').color('Linear', 0, 1000, 255, 0);",
                    // loops and triggers
                    "SceneApi.sprite('a.png').loop(0, '2', l => {});",
                    "SceneApi.sprite('a.png').loop(0, 2);",
                    "SceneApi.sprite('a.png').loop();",
                    "SceneApi.sprite('a.png').trigger('Passing', 0, undefined, t => {});",
                    "SceneApi.sprite('a.png').trigger(5, 0, 1000, t => {});",
                    "SceneApi.sprite('a.png').trigger('Passing', 0, 1000, t => {}, '1');"}){
                script_host host;
                const auto error = host.evaluate(QString("\n%1\n").arg(call));

                QVERIFY2(error.has_value(), call);
                QCOMPARE(error->line, 2);

                // Nothing half-made: no command, loop or trigger was stored.
                for(const sprite& s : host.current_scene().sprites){
                    QVERIFY2(s.cmds.isEmpty() && s.triggers.isEmpty(), call);
                    for(const loop_group& l : s.loops){
                        QVERIFY2(l.cmds.isEmpty(), call);
                    }
                }
                QVERIFY2(host.current_scene().samples.isEmpty(), call);
            }
        }

        void correct_scripts_still_run(){
            script_host host;

            const auto error = host.evaluate(
                    "const s = SceneApi.sprite('a.png', { layer: 'background', origin: 'Centre', x: 10, y: 20 });\n"
                    "SceneApi.sprite('b.png', { origin: 'middle' });\n"
                    "SceneApi.sprite('c.png', { origin: 'CenterCenter', position: [1, 2], name: 'c' });\n"
                    "SceneApi.add_sprite('d.png', 'OVERLAY', 'bottomright', 0, 0);\n"
                    "SceneApi.sprite('e.png');\n"
                    "s.fade({ startTime: 0, endTime: 1000, startValue: 0, endValue: 1, easing: 3 })\n"
                    "    .fade('QuadOut', 1000, 2000, 1, 0)\n"
                    "    .move('Linear', 0, 500, 0, 0, 100, 100)\n"
                    "    .move({ startTime: 0, endTime: 500, startPosition: [0, 0], endX: 10 })\n"
                    "    .scale('Linear', 0, 500, 1, 2)\n"
                    "    .rotate({ startTime: 0, endTime: 500, endValue: 3.14 })\n"
                    "    .color('Linear', 0, 500, 255, 0, 0)\n"
                    "    .color({ startTime: 0, r: 0, g: 128, b: 255 })\n"
                    "    .scaleVec({ startTime: 0, endTime: 500, endValue: [2, 1] })\n"
                    "    .flipH({ startTime: 1000 })\n"
                    "    .additive({ startTime: 500, endTime: 2000 })\n"
                    "    .fade({ startTime: 1000.7, endTime: 1000.2 })\n"
                    "    .loop(0, 2, l => l.fade({ startTime: 0, endTime: 100 }))\n"
                    "    .trigger('HitSoundClap', 0, 1000, t => t.scale({ startTime: 0, endTime: 100 }))\n"
                    "    .trigger('Passing', 0, 1000, t => t.fade({ endTime: 10 }), 1);\n"
                    "SceneApi.sample(0, 'a.wav');\n"
                    "SceneApi.sample(10, 'b.wav', { layer: 'pass', volume: 50 });\n"
                    "SceneApi.animation('f.png', { frameCount: 2, frameDelay: 10, loopType: 'loopOnce' });\n");

            QVERIFY2(!error.has_value(), error ? qPrintable(error->message) : "");

            const scene& sc = host.current_scene();
            QCOMPARE(sc.sprites.size(), qsizetype(6));
            QCOMPARE(sc.sprites[0].layer, layer_t::background);
            QCOMPARE(sc.sprites[0].origin, origin_t::center);
            QCOMPARE(sc.sprites[1].origin, origin_t::center);
            QCOMPARE(sc.sprites[2].origin, origin_t::center);
            QCOMPARE(sc.sprites[2].x, 1.0);
            QCOMPARE(sc.sprites[3].layer, layer_t::overlay);
            QCOMPARE(sc.sprites[3].origin, origin_t::bottom_right);
            QCOMPARE(sc.sprites[4].x, 320.0);
            QCOMPARE(sc.sprites[4].layer, layer_t::foreground);
            QCOMPARE(sc.sprites[0].cmds.size(), qsizetype(12));
            QCOMPARE(sc.sprites[0].loops.size(), qsizetype(1));
            QCOMPARE(sc.sprites[0].triggers.size(), qsizetype(2));
            QCOMPARE(sc.sprites[0].triggers[1].group, 1);
            QCOMPARE(sc.samples.size(), qsizetype(2));
        }

        void leaving_out_end_time_makes_an_instant_command(){
            script_host host;

            QVERIFY(!host.evaluate(
                    "SceneApi.sprite('a.png')\n"
                    "    .fade({ startTime: 1000, startValue: 1, endValue: 1 })\n"
                    "    .move({ startTime: 500, x: 1, y: 2 })\n"
                    "    .moveX({ startTime: 200 })\n"
                    "    .flipH({ startTime: 1000 });\n").has_value());

            const auto& cmds = host.current_scene().sprites.first().cmds;
            QCOMPARE(cmds.size(), qsizetype(4));
            QCOMPARE(cmds[0].t0, 1000);
            QCOMPARE(cmds[0].t1, 1000);
            QCOMPARE(cmds[1].t1, 500);
            QCOMPARE(cmds[1].x1, 1.0);
            QCOMPARE(cmds[2].t1, 200);
            QCOMPARE(cmds[2].v1, 320.0);    // the sprite's x
            // flipH/flipV/additive keep 0, which osu! reads as "from startTime on".
            QCOMPARE(cmds[3].t1, 0);
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
