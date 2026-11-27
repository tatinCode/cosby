#include "timeline_view.h"
#include "ui/timeline_model.h"
#include "core/time_format.h"
#include "cosby_test.h"

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QLineEdit>
#include <QSignalSpy>
#include <QtTest>

// The timeline: its row model, the widget, and the time box's parsing.

namespace {
    command make_command(cmd_t type, int t0, int t1){
        command cmd;
        cmd.type = type;
        cmd.t0 = t0;
        cmd.t1 = t1;
        return cmd;
    }

    qsizetype add(scene& sc, const QString& path, layer_t layer, int line,
            QVector<command> cmds = {}, const QString& name = QString()){
        const qsizetype index = sc.add_sprite(path, layer, origin_t::center, 320, 240);
        sc.sprites[index].source_line = line;
        sc.sprites[index].name = name;
        sc.sprites[index].cmds = std::move(cmds);
        return index;
    }

    QStringList labels(const timeline_model& model){
        QStringList out;
        for(const timeline_row& row : model.rows()){
            out << QString(row.depth * 2, ' ') + row.label;
        }
        return out;
    }

    int find_row(const timeline_model& model, const QString& label){
        const auto& rows = model.rows();
        for(int i = 0; i < rows.size(); ++i){
            if(rows.at(i).label == label){
                return i;
            }
        }
        return -1;
    }
}

class TestTimelineModel : public QObject {
    Q_OBJECT

    private slots:
        void empty_scene_has_no_rows(){
            timeline_model model;
            model.set_scene(scene{});

            QVERIFY(model.rows().isEmpty());
            QVERIFY(model.is_empty());
            QVERIFY(model.time_begin() > model.time_end());
        }

        void layers_follow_osu_draw_order(){
            scene sc;
            add(sc, "over.png", layer_t::overlay, 1);
            add(sc, "front.png", layer_t::foreground, 2);
            add(sc, "bg.jpg", layer_t::background, 3);

            timeline_model model;
            model.set_scene(sc);

            QCOMPARE(labels(model), (QStringList{
                "Background", "  bg.jpg",
                "Foreground", "  front.png",
                "Overlay", "  over.png"}));
            QCOMPARE(model.rows().first().detail, QString("1 sprite"));
        }

        void same_call_site_is_grouped(){
            scene sc;
            add(sc, "bg.jpg", layer_t::background, 1);
            for(int i = 0; i < 3; ++i){
                add(sc, "sb/star.png", layer_t::foreground, 5, {make_command(cmd_t::fade, i * 100, i * 100 + 50)});
            }

            timeline_model model;
            model.set_scene(sc);

            QCOMPARE(labels(model), (QStringList{
                "Background", "  bg.jpg",
                "Foreground", "  star.png ×3"}));

            const timeline_row& group = model.rows().at(3);
            QCOMPARE(group.kind, timeline_row_kind::group);
            QCOMPARE(group.sprite_count, qsizetype(3));
            QCOMPARE(group.source_line, 5);
            QCOMPARE(group.detail, QString("line 5"));
            QVERIFY(group.expandable);
            QVERIFY(!group.expanded);   // collapsed by default
            QCOMPARE(group.bars.size(), qsizetype(3));  // no overlap: three spans
        }

        void different_names_or_files_split_groups(){
            scene sc;
            add(sc, "a.png", layer_t::foreground, 5, {}, "left");
            add(sc, "a.png", layer_t::foreground, 5, {}, "right");
            add(sc, "b.png", layer_t::foreground, 5);

            timeline_model model;
            model.set_scene(sc);

            QCOMPARE(labels(model), (QStringList{"Foreground", "  left", "  right", "  b.png"}));
        }

        void unknown_line_is_never_grouped(){
            scene sc;
            add(sc, "a.png", layer_t::foreground, 0);
            add(sc, "a.png", layer_t::foreground, 0);

            timeline_model model;
            model.set_scene(sc);

            QCOMPARE(labels(model), (QStringList{"Foreground", "  a.png", "  a.png"}));
            QVERIFY(model.rows().at(1).key != model.rows().at(2).key);
            QCOMPARE(model.rows().at(1).detail, QString());
        }

        void groups_keep_first_appearance_order(){
            scene sc;
            add(sc, "a.png", layer_t::foreground, 1);
            add(sc, "b.png", layer_t::foreground, 2);
            add(sc, "a.png", layer_t::foreground, 1);

            timeline_model model;
            model.set_scene(sc);

            QCOMPARE(labels(model), (QStringList{"Foreground", "  a.png ×2", "  b.png"}));
        }

        void group_lifetimes_are_merged(){
            scene sc;
            add(sc, "p.png", layer_t::foreground, 3, {make_command(cmd_t::fade, 0, 100)});
            add(sc, "p.png", layer_t::foreground, 3, {make_command(cmd_t::fade, 50, 200)});
            add(sc, "p.png", layer_t::foreground, 3, {make_command(cmd_t::move, 300, 400)});
            add(sc, "p.png", layer_t::foreground, 3);  // no commands: no bar

            timeline_model model;
            model.set_scene(sc);

            const auto& bars = model.rows().at(1).bars;
            QCOMPARE(bars.size(), qsizetype(2));
            QCOMPARE(bars.at(0).t0, 0);
            QCOMPARE(bars.at(0).t1, 200);
            QCOMPARE(bars.at(1).t0, 300);
            QCOMPARE(bars.at(1).t1, 400);
            QVERIFY(!bars.at(0).is_command());
        }

        void expanding_shows_members_and_command_lanes(){
            scene sc;
            add(sc, "p.png", layer_t::foreground, 3, {
                    make_command(cmd_t::scale, 0, 100),
                    make_command(cmd_t::fade, 0, 50),
                    make_command(cmd_t::fade, 500, 600)});
            add(sc, "p.png", layer_t::foreground, 3, {make_command(cmd_t::move, 0, 10)});

            timeline_model model;
            model.set_scene(sc);

            model.toggle_expanded(find_row(model, "p.png ×2"));
            QCOMPARE(labels(model), (QStringList{"Foreground", "  p.png ×2", "    p.png", "    p.png"}));

            model.toggle_expanded(2);
            // Lanes in enum order (fade before scale), one per type used.
            QCOMPARE(labels(model), (QStringList{
                "Foreground", "  p.png ×2",
                "    p.png", "      Fade", "      Scale",
                "    p.png"}));

            const timeline_row& fade = model.rows().at(3);
            QCOMPARE(fade.kind, timeline_row_kind::command_lane);
            QCOMPARE(fade.command_type, cmd_t::fade);
            QCOMPARE(fade.bars.size(), qsizetype(2));
            QVERIFY(fade.bars.at(0).is_command());
            QCOMPARE(fade.bars.at(1).command, qsizetype(2));
            QCOMPARE(fade.bars.at(1).sprite, qsizetype(0));
        }

        void collapsing_a_layer_hides_its_sprites(){
            scene sc;
            add(sc, "bg.jpg", layer_t::background, 1);
            add(sc, "front.png", layer_t::foreground, 2);

            timeline_model model;
            model.set_scene(sc);
            model.toggle_expanded(0);

            QCOMPARE(labels(model), (QStringList{"Background", "Foreground", "  front.png"}));
            QVERIFY(!model.rows().at(0).expanded);
        }

        void expand_state_survives_a_rerun(){
            auto build = []{
                scene sc;
                add(sc, "p.png", layer_t::foreground, 3, {make_command(cmd_t::fade, 0, 1)});
                add(sc, "p.png", layer_t::foreground, 3, {make_command(cmd_t::fade, 0, 1)});
                return sc;
            };

            timeline_model model;
            model.set_scene(build());
            model.toggle_expanded(1);
            model.toggle_expanded(2);   // first member
            const QStringList before = labels(model);

            // A new run: a different scene object with the same content.
            model.set_scene(build());

            QCOMPARE(labels(model), before);
            QVERIFY(model.rows().at(1).expanded);
            QVERIFY(model.rows().at(2).expanded);
        }

        void sprites_without_commands_cannot_expand(){
            scene sc;
            add(sc, "a.png", layer_t::foreground, 1);

            timeline_model model;
            model.set_scene(sc);
            model.toggle_expanded(1);

            QVERIFY(!model.rows().at(1).expandable);
            QCOMPARE(model.rows().size(), qsizetype(2));
            QVERIFY(model.rows().at(1).bars.isEmpty());
        }

        void time_range_covers_all_commands(){
            scene sc;
            add(sc, "a.png", layer_t::foreground, 1, {make_command(cmd_t::fade, 500, 900)});
            // An instant / "from t0 on" flag counts as its start time.
            add(sc, "b.png", layer_t::overlay, 2, {make_command(cmd_t::flip_h, 2000, 0)});
            add(sc, "c.png", layer_t::background, 3, {make_command(cmd_t::move, -1000, 100)});

            timeline_model model;
            model.set_scene(sc);

            QCOMPARE(model.time_begin(), -1000);
            QCOMPARE(model.time_end(), 2000);
        }

        void model_keeps_its_own_copy(){
            scene sc;
            add(sc, "a.png", layer_t::foreground, 1, {make_command(cmd_t::fade, 0, 100)});

            timeline_model model;
            model.set_scene(sc);

            // The script host resets its scene on the next run.
            sc = scene{};

            QCOMPARE(model.current_scene().sprites.size(), qsizetype(1));
            QCOMPARE(model.current_scene().sprites.first().cmds.size(), qsizetype(1));
        }

        void named_sprite_uses_its_name(){
            scene sc;
            add(sc, "sb/very/long/path/flash.png", layer_t::foreground, 1, {}, "beat flash");

            timeline_model model;
            model.set_scene(sc);

            QCOMPARE(model.rows().at(1).label, QString("beat flash"));
        }

        void labels_with_percent_signs_are_literal(){
            scene sc;
            add(sc, "100%25%2.png", layer_t::foreground, 1);
            add(sc, "100%25%2.png", layer_t::foreground, 1);

            timeline_model model;
            model.set_scene(sc);

            QCOMPARE(model.rows().at(1).label, QString("100%25%2.png ×2"));
        }
};

COSBY_TEST(TestTimelineModel)

class TestTimeFormat : public QObject {
    Q_OBJECT

    private slots:
        void formats_osu_timestamps(){
            QCOMPARE(format_osu_timestamp(0), QString("00:00:000"));
            QCOMPARE(format_osu_timestamp(83456), QString("01:23:456"));
            QCOMPARE(format_osu_timestamp(7), QString("00:00:007"));
            QCOMPARE(format_osu_timestamp(61 * 60000 + 5), QString("61:00:005"));
            QCOMPARE(format_osu_timestamp(-250), QString("-00:00:250"));
        }

        void parses_accepted_formats_data(){
            QTest::addColumn<QString>("input");
            QTest::addColumn<qint64>("current");
            QTest::addColumn<qint64>("expected");

            QTest::newRow("ms") << "83456" << qint64(0) << qint64(83456);
            QTest::newRow("ms, spaces") << "  83456 " << qint64(0) << qint64(83456);
            QTest::newRow("zero") << "0" << qint64(5000) << qint64(0);
            QTest::newRow("m:ss.mmm") << "1:23.456" << qint64(0) << qint64(83456);
            QTest::newRow("mm:ss.mmm") << "01:23.456" << qint64(0) << qint64(83456);
            QTest::newRow("m:ss") << "1:23" << qint64(0) << qint64(83000);
            QTest::newRow("m:s") << "1:5" << qint64(0) << qint64(65000);
            QTest::newRow("fraction .5") << "0:01.5" << qint64(0) << qint64(1500);
            QTest::newRow("fraction .05") << "0:01.05" << qint64(0) << qint64(1050);
            QTest::newRow("osu") << "01:23:456" << qint64(0) << qint64(83456);
            QTest::newRow("osu copy") << "01:23:456 (1,2,3) - " << qint64(0) << qint64(83456);
            QTest::newRow("osu copy, no objects") << "00:10:000 - " << qint64(0) << qint64(10000);
            QTest::newRow("seconds") << "12.5s" << qint64(0) << qint64(12500);
            QTest::newRow("seconds, space") << "3 s" << qint64(0) << qint64(3000);
            QTest::newRow("plus ms") << "+500" << qint64(1000) << qint64(1500);
            QTest::newRow("minus ms") << "-250" << qint64(1000) << qint64(750);
            QTest::newRow("minus minute") << "-1:00" << qint64(90000) << qint64(30000);
            QTest::newRow("plus seconds") << "+ 2s" << qint64(0) << qint64(2000);
            QTest::newRow("relative below 0") << "-500" << qint64(100) << qint64(-400);
        }

        void parses_accepted_formats(){
            QFETCH(QString, input);
            QFETCH(qint64, current);
            QFETCH(qint64, expected);

            const auto result = parse_time_input(input, current);
            QVERIFY2(result.has_value(), qPrintable(input));
            QCOMPARE(*result, expected);
        }

        void rejects_garbage_data(){
            QTest::addColumn<QString>("input");

            QTest::newRow("empty") << "";
            QTest::newRow("spaces") << "   ";
            QTest::newRow("word") << "abc";
            QTest::newRow("sign only") << "+";
            QTest::newRow("seconds over 59") << "1:60";
            QTest::newRow("osu seconds over 59") << "00:75:000";
            QTest::newRow("fraction too long") << "1:23.4567";
            QTest::newRow("decimal ms") << "123.4";
            QTest::newRow("three colons") << "1:2:3:4";
            QTest::newRow("trailing junk") << "1000ms";
            QTest::newRow("beyond 24h") << "86400001";
            QTest::newRow("huge") << "99999999999999";
            QTest::newRow("double sign") << "+-5";
        }

        void rejects_garbage(){
            QFETCH(QString, input);
            QVERIFY2(!parse_time_input(input, 1000).has_value(), qPrintable(input));
        }

        void round_trips(){
            for(qint64 ms : {qint64(0), qint64(1), qint64(59999), qint64(60000), qint64(3723004)}){
                QCOMPARE(parse_time_input(format_osu_timestamp(ms), 0), std::optional<qint64>(ms));
                QCOMPARE(parse_time_input(QString::number(ms), 0), std::optional<qint64>(ms));
            }
        }
};

COSBY_TEST(TestTimeFormat)

namespace {
    // Layout constants mirrored from TimelineView.
    constexpr int ruler_height = 24;
    constexpr int row_height = 22;
    constexpr int header_width = 220;
    constexpr int footer_height = 24;

    int row_y(int row){
        return ruler_height + row * row_height + row_height / 2;
    }

    command fade(int t0, int t1){
        command cmd;
        cmd.type = cmd_t::fade;
        cmd.t0 = t0;
        cmd.t1 = t1;
        return cmd;
    }

    // Rows (all collapsed groups):
    //   0 Background   1 bg.jpg (line 1)
    //   2 Foreground   3 p.png ×3 (line 2)
    //   4 Overlay      5 flash (line 7)
    scene sample_scene(){
        scene sc;

        auto add = [&sc](const QString& path, layer_t layer, int line, const QString& name = {}){
            const qsizetype i = sc.add_sprite(path, layer, origin_t::center, 0, 0);
            sc.sprites[i].source_line = line;
            sc.sprites[i].name = name;
            sc.sprites[i].cmds = {fade(0, 4000)};
        };

        add("bg.jpg", layer_t::background, 1);
        for(int i = 0; i < 3; ++i){
            add("p.png", layer_t::foreground, 2);
        }
        add("f.png", layer_t::overlay, 7, "flash");

        return sc;
    }

    void activate(QWidget& w){
        w.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&w));
    }

    // Opens the time box by clicking the time, like a user would.
    QLineEdit* open_field(TimelineView& view){
        QTest::mouseClick(&view, Qt::LeftButton, {}, view.timeLabelRect().center());
        return view.timeField();
    }
}

class TestTimelineView : public QObject {
    Q_OBJECT

    private:
        std::unique_ptr<TimelineView> make_view(){
            auto view = std::make_unique<TimelineView>();
            view->resize(900, 400);
            view->setBpm(120);          // 500 ms per beat
            view->setBeatOffsetMs(100);
            view->setSubdivision(1);
            view->show();

            const scene sc = sample_scene();
            view->setScene(&sc);

            return view;
        }

    private slots:
        void clicking_the_arrow_expands_a_group(){
            auto view = make_view();
            QCOMPARE(view->model().rows().size(), qsizetype(6));

            QTest::mouseClick(view.get(), Qt::LeftButton, {}, QPoint(6 + 14 + 4, row_y(3)));

            QCOMPARE(view->model().rows().size(), qsizetype(9));
            QVERIFY(view->model().rows().at(3).expanded);
        }

        void clicking_a_label_only_selects(){
            auto view = make_view();

            QTest::mouseClick(view.get(), Qt::LeftButton, {}, QPoint(120, row_y(3)));

            QCOMPARE(view->model().rows().size(), qsizetype(6));
        }

        void double_click_opens_the_creating_line(){
            auto view = make_view();
            QSignalSpy spy(view.get(), &TimelineView::sourceLineActivated);

            QTest::mouseDClick(view.get(), Qt::LeftButton, {}, QPoint(120, row_y(3)));
            QTest::mouseDClick(view.get(), Qt::LeftButton, {}, QPoint(header_width + 200, row_y(5)));

            QCOMPARE(spy.count(), 2);
            QCOMPARE(spy.at(0).at(0).toInt(), 2);
            QCOMPARE(spy.at(1).at(0).toInt(), 7);
        }

        void double_click_on_a_layer_toggles_it(){
            auto view = make_view();
            QSignalSpy spy(view.get(), &TimelineView::sourceLineActivated);

            QTest::mouseDClick(view.get(), Qt::LeftButton, {}, QPoint(120, row_y(0)));

            QCOMPARE(spy.count(), 0);
            QVERIFY(!view->model().rows().at(0).expanded);
        }

        void eye_hides_and_shows_a_layer(){
            auto view = make_view();
            QSignalSpy spy(view.get(), &TimelineView::layerVisibilityChanged);

            const QPoint eye(header_width - 24 + 9, row_y(0));
            QTest::mouseClick(view.get(), Qt::LeftButton, {}, eye);

            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.at(0).at(0).value<layer_t>(), layer_t::background);
            QCOMPARE(spy.at(0).at(1).toBool(), false);

            // The eye doesn't expand/collapse the layer.
            QVERIFY(view->model().rows().at(0).expanded);

            view->showAllLayers();
            QCOMPARE(spy.count(), 2);
            QCOMPARE(spy.at(1).at(1).toBool(), true);
        }

        void clicking_the_tracks_seeks_on_the_beat_grid(){
            auto view = make_view();
            QSignalSpy spy(view.get(), &TimelineView::requestSeek);

            for(int x : {header_width + 37, header_width + 211, header_width + 400}){
                QTest::mouseClick(view.get(), Qt::LeftButton, {}, QPoint(x, row_y(1)));
            }

            QCOMPARE(spy.count(), 3);

            for(const auto& call : spy){
                const qint64 ms = call.at(0).toLongLong();
                // Beats are at 100 + k * 500.
                QCOMPARE(((ms - 100) % 500 + 500) % 500, qint64(0));
            }

            // Alt turns snapping off.
            QTest::mouseClick(view.get(), Qt::LeftButton, Qt::AltModifier, QPoint(header_width + 211, row_y(1)));
            QCOMPARE(spy.count(), 4);
        }

        void dragging_keeps_seeking(){
            auto view = make_view();
            QSignalSpy spy(view.get(), &TimelineView::requestSeek);

            QTest::mousePress(view.get(), Qt::LeftButton, Qt::AltModifier, QPoint(header_width + 50, 10));
            QTest::mouseMove(view.get(), QPoint(header_width + 300, 10));
            QTest::mouseRelease(view.get(), Qt::LeftButton, Qt::AltModifier, QPoint(header_width + 300, 10));

            QVERIFY(spy.count() >= 2);
            QVERIFY(spy.last().at(0).toLongLong() > spy.first().at(0).toLongLong());
        }

        void first_scene_is_fitted(){
            auto view = make_view();
            QSignalSpy spy(view.get(), &TimelineView::requestSeek);

            // Fitted to 0..4000 ms: the right edge of the tracks is near 4000.
            QTest::mouseClick(view.get(), Qt::LeftButton, Qt::AltModifier, QPoint(view->width() - 2, 10));

            QCOMPARE(spy.count(), 1);
            const qint64 ms = spy.first().at(0).toLongLong();
            QVERIFY2(ms > 3900 && ms < 4300, qPrintable(QString::number(ms)));
        }

        void clearing_shows_no_rows(){
            auto view = make_view();
            view->setScene(nullptr);

            QVERIFY(view->model().rows().isEmpty());

            // Clicks on an empty timeline are harmless.
            QTest::mouseDClick(view.get(), Qt::LeftButton, {}, QPoint(120, row_y(0)));
        }

        void wheel_and_zoom_do_not_lose_rows(){
            auto view = make_view();
            // 56 px of tracks for 132 px of rows: one wheel notch (66 px) fits.
            view->resize(900, 80);

            QWheelEvent down(QPointF(400, 60), view->mapToGlobal(QPointF(400, 60)), {}, QPoint(0, -120),
                    Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(view.get(), &down);

            QWheelEvent zoom(QPointF(400, 60), view->mapToGlobal(QPointF(400, 60)), {}, QPoint(0, 120),
                    Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(view.get(), &zoom);

            // Scrolled down by 3 rows: the first visible row is now "p.png ×3" (line 2).
            QSignalSpy spy(view.get(), &TimelineView::sourceLineActivated);
            QTest::mouseDClick(view.get(), Qt::LeftButton, {}, QPoint(120, row_y(0)));
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.first().at(0).toInt(), 2);
        }

        // Footer: "00:12:345" as plain text; clicking opens a box with the ms.

        void the_time_is_text_until_clicked(){
            auto view = make_view();
            view->setPlayheadMs(83456);

            QVERIFY(!view->timeField()->isVisible());
            QVERIFY(view->timeLabelRect().top() >= view->height() - footer_height);
            QVERIFY(view->timeLabelRect().right() < header_width);
        }

        void clicking_the_time_opens_it_in_ms_without_copying(){
            auto view = make_view();
            activate(*view);
            view->setPlayheadMs(83456);
            QGuiApplication::clipboard()->setText("untouched");

            QTest::mouseClick(view.get(), Qt::LeftButton, {}, view->timeLabelRect().center());

            QLineEdit* field = view->timeField();
            QVERIFY(field->isVisible());
            QVERIFY(field->hasFocus());
            QCOMPARE(field->text(), QString("83456"));
            QCOMPARE(field->selectedText(), field->text());
            QCOMPARE(QGuiApplication::clipboard()->text(), QString("untouched"));
        }

        void typing_a_time_and_enter_seeks_exactly(){
            auto view = make_view();
            QSignalSpy spy(view.get(), &TimelineView::requestSeek);
            QLineEdit* field = open_field(*view);

            QTest::keyClicks(field, "1:02.5");      // replaces the selection
            QTest::keyClick(field, Qt::Key_Return);

            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.first().at(0).toLongLong(), qint64(62500));   // not snapped
            QVERIFY(!field->isVisible());
        }

        void osu_timestamps_can_be_pasted(){
            auto view = make_view();
            QSignalSpy spy(view.get(), &TimelineView::requestSeek);
            QLineEdit* field = open_field(*view);

            QTest::keyClicks(field, "00:12:345 (1,2) - ");
            QTest::keyClick(field, Qt::Key_Enter);

            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.first().at(0).toLongLong(), qint64(12345));
        }

        void invalid_input_stays_open_and_does_not_seek(){
            auto view = make_view();
            QSignalSpy spy(view.get(), &TimelineView::requestSeek);
            QLineEdit* field = open_field(*view);

            QTest::keyClicks(field, "soon");
            QTest::keyClick(field, Qt::Key_Return);

            QCOMPARE(spy.count(), 0);
            QVERIFY(field->isVisible());
            QVERIFY(field->property("invalid").toBool());
            QCOMPARE(field->text(), QString("soon"));      // kept, so it can be fixed

            QTest::keyClick(field, Qt::Key_Backspace);
            QVERIFY(!field->property("invalid").toBool());
        }

        void escape_closes_without_seeking(){
            auto view = make_view();
            QSignalSpy spy(view.get(), &TimelineView::requestSeek);
            QLineEdit* field = open_field(*view);

            QTest::keyClicks(field, "999");
            QTest::keyClick(field, Qt::Key_Escape);

            QCOMPARE(spy.count(), 0);
            QVERIFY(!field->isVisible());
        }

        void clicking_elsewhere_closes_it(){
            auto view = make_view();
            activate(*view);
            QLineEdit* field = open_field(*view);

            QTest::keyClicks(field, "77");
            view->setFocus();

            QTRY_VERIFY(!field->isVisible());
        }

        void playback_does_not_overwrite_typing(){
            auto view = make_view();
            QLineEdit* field = open_field(*view);

            QTest::keyClicks(field, "12");
            view->setPlayheadMs(5000);

            QCOMPARE(field->text(), QString("12"));
        }

        void playback_keeps_the_ms_selected(){
            auto view = make_view();
            QLineEdit* field = open_field(*view);

            view->setPlayheadMs(5000);

            QCOMPARE(field->text(), QString("5000"));
            QCOMPARE(field->selectedText(), field->text());
        }

        void arrows_step_on_the_grid(){
            auto view = make_view();
            view->setSubdivision(4);    // 125 ms ticks from 100
            QSignalSpy spy(view.get(), &TimelineView::requestSeek);
            QLineEdit* field = open_field(*view);

            view->setPlayheadMs(100);
            QTest::keyClick(field, Qt::Key_Up);
            QCOMPARE(field->text(), QString("225"));

            view->setPlayheadMs(130);  // off the grid
            QTest::keyClick(field, Qt::Key_Down);
            view->setPlayheadMs(100);
            QTest::keyClick(field, Qt::Key_Up, Qt::ShiftModifier);
            view->setPlayheadMs(130);
            QTest::keyClick(field, Qt::Key_Down, Qt::ShiftModifier);

            QCOMPARE(spy.count(), 4);
            QCOMPARE(spy.at(0).at(0).toLongLong(), qint64(225));
            QCOMPARE(spy.at(1).at(0).toLongLong(), qint64(100));
            QCOMPARE(spy.at(2).at(0).toLongLong(), qint64(600));
            QCOMPARE(spy.at(3).at(0).toLongLong(), qint64(100));
            QVERIFY(field->isVisible());   // stays open for more steps
        }

        void arrows_do_not_get_stuck_on_fractional_ticks(){
            auto view = make_view();
            view->setBpm(180);          // 333.33 ms beats
            view->setBeatOffsetMs(0);
            view->setSubdivision(2);    // 166.67 ms ticks
            QSignalSpy spy(view.get(), &TimelineView::requestSeek);
            QLineEdit* field = open_field(*view);

            view->setPlayheadMs(167);   // tick 1, rounded
            QTest::keyClick(field, Qt::Key_Down);
            view->setPlayheadMs(167);
            QTest::keyClick(field, Qt::Key_Up);

            QCOMPARE(spy.at(0).at(0).toLongLong(), qint64(0));
            QCOMPARE(spy.at(1).at(0).toLongLong(), qint64(333));
        }

        void space_types_in_the_box_but_plays_elsewhere(){
            auto view = make_view();
            QAction play(view.get());
            play.setShortcut(Qt::Key_Space);
            play.setShortcutContext(Qt::WidgetWithChildrenShortcut);
            view->addAction(&play);
            QSignalSpy triggered(&play, &QAction::triggered);
            activate(*view);

            // Control: on the timeline itself, Space plays.
            view->setFocus();
            QTest::keyClick(view.get(), Qt::Key_Space);
            QCOMPARE(triggered.count(), 1);

            QLineEdit* field = open_field(*view);
            field->end(false);
            QTest::keyClick(field, Qt::Key_Space);

            QCOMPARE(triggered.count(), 1);
            QCOMPARE(field->text(), QString("0 "));
        }

        void ctrl_g_opens_the_box(){
            auto view = make_view();
            activate(*view);
            view->setPlayheadMs(4321);

            view->setFocus();
            QTest::keyClick(view.get(), Qt::Key_G, Qt::ControlModifier);

            QLineEdit* field = view->timeField();
            QVERIFY(field->isVisible());
            QVERIFY(field->hasFocus());
            QCOMPARE(field->selectedText(), QString("4321"));
        }

        void footer_clicks_beside_the_time_do_nothing(){
            auto view = make_view();
            QSignalSpy seek(view.get(), &TimelineView::requestSeek);
            QSignalSpy open(view.get(), &TimelineView::sourceLineActivated);

            const QPoint in_footer(header_width + 100, view->height() - footer_height / 2);
            QTest::mouseClick(view.get(), Qt::LeftButton, {}, in_footer);
            QTest::mouseDClick(view.get(), Qt::LeftButton, {}, in_footer);

            QCOMPARE(seek.count(), 0);
            QCOMPARE(open.count(), 0);
            QVERIFY(!view->timeField()->isVisible());
        }
};

COSBY_TEST(TestTimelineView)

#include "test_timeline.moc"
