#include "core/osb_writer.h"
#include "project/asset_paths.h"
#include "project/storyboard_export.h"
#include "project/asset_check.h"
#include "cosby_test.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>

namespace {
    command make_command(cmd_t type, int t0, int t1, double v0 = 0.0, double v1 = 0.0, easing_t easing = easing_t::linear){
        command cmd;
        cmd.type = type;
        cmd.easing = easing;
        cmd.t0 = t0;
        cmd.t1 = t1;
        cmd.v0 = v0;
        cmd.v1 = v1;
        return cmd;
    }

    // The sprite's lines of a single-sprite scene (header and commands).
    QStringList sprite_lines(const sprite& s){
        scene sc;
        sc.sprites.push_back(s);

        QStringList lines = write_osb(sc).split('\n', Qt::SkipEmptyParts);
        lines.removeIf([](const QString& line){ return line.startsWith("//") || line.startsWith('['); });
        return lines;
    }

    sprite faded_sprite(QString path = "sb/a.png"){
        sprite s;
        s.path = std::move(path);
        s.x = 320;
        s.y = 240;
        s.cmds.push_back(make_command(cmd_t::fade, 0, 1000, 0, 1));
        return s;
    }

    bool write_file(const QString& path, const QByteArray& data){
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    }

    QByteArray read_file(const QString& path){
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
}

class TestOsbWriter : public QObject{
    Q_OBJECT

    private slots:
        void writes_layers_in_osu_order(){
            scene sc;

            // Created foreground first: the file is still ordered by layer.
            sc.add_sprite("sb/a.png", layer_t::foreground, origin_t::center, 320, 240);
            sc.sprites[0].cmds.push_back(make_command(cmd_t::fade, 0, 1000, 0, 1));

            sc.add_sprite("bg.jpg", layer_t::background, origin_t::top_left, 0, 0);
            command move = make_command(cmd_t::move, 0, 500, 0, 0, easing_t::out);
            move.x1 = 10.5;
            move.y1 = -3;
            sc.sprites[1].cmds.push_back(move);

            QCOMPARE(write_osb(sc), QString(
                "[Events]\n"
                "//Background and Video events\n"
                "//Storyboard Layer 0 (Background)\n"
                "Sprite,Background,TopLeft,\"bg.jpg\",0,0\n"
                " M,1,0,500,0,0,10.5,-3\n"
                "//Storyboard Layer 1 (Fail)\n"
                "//Storyboard Layer 2 (Pass)\n"
                "//Storyboard Layer 3 (Foreground)\n"
                "Sprite,Foreground,Centre,\"sb/a.png\",320,240\n"
                " F,0,0,1000,0,1\n"
                "//Storyboard Layer 4 (Overlay)\n"
                "//Storyboard Sound Samples\n"));
        }

        void keeps_creation_order_within_a_layer(){
            scene sc;
            sc.sprites << faded_sprite("one.png") << faded_sprite("two.png");

            const QString text = write_osb(sc);
            QVERIFY(text.indexOf("one.png") < text.indexOf("two.png"));
        }

        void writes_every_command(){
            sprite s;
            s.path = "a.png";

            s.cmds << make_command(cmd_t::scale, 0, 100, 1, 2, easing_t::in)
                   << make_command(cmd_t::rotate, 0, 100, 0, 3.14159265)
                   << make_command(cmd_t::move_x, 0, 100, 1, 2)
                   << make_command(cmd_t::move_y, 0, 100, 3, 4);

            command vec = make_command(cmd_t::scale_vec, 0, 100);
            vec.x0 = 1; vec.y0 = 1; vec.x1 = 2; vec.y1 = 0.5;
            s.cmds << vec;

            command colour = make_command(cmd_t::color, 0, 100);
            colour.r0 = 255; colour.g0 = 0; colour.b0 = 0;
            colour.r1 = 0; colour.g1 = 0; colour.b1 = 255;
            s.cmds << colour;

            s.cmds << make_command(cmd_t::flip_h, 100, 200)
                   << make_command(cmd_t::flip_v, 300, 300)
                   << make_command(cmd_t::additive, 400, 0);   // permanent

            QCOMPARE(sprite_lines(s), QStringList({
                "Sprite,Foreground,Centre,\"a.png\",0,0",
                " S,2,0,100,1,2",
                " R,0,0,100,0,3.1416",
                " MX,0,0,100,1,2",
                " MY,0,0,100,3,4",
                " V,0,0,100,1,1,2,0.5",
                " C,0,0,100,255,0,0,0,0,255",
                " P,0,100,200,H",
                " P,0,300,300,V",
                " P,0,400,400,A",
            }));
        }

        void leaves_out_equal_end_values(){
            sprite s;
            s.path = "a.png";
            s.cmds << make_command(cmd_t::fade, 0, 100, 1, 1);

            command colour = make_command(cmd_t::color, 0, 100);
            colour.r0 = colour.r1 = 10;
            colour.g0 = colour.g1 = 20;
            colour.b0 = colour.b1 = 30;
            s.cmds << colour;

            QCOMPARE(sprite_lines(s).mid(1), QStringList({" F,0,0,100,1", " C,0,0,100,10,20,30"}));
        }

        void formats_numbers(){
            QCOMPARE(osb_number(320), QString("320"));
            QCOMPARE(osb_number(-107.5), QString("-107.5"));
            QCOMPARE(osb_number(1.23456), QString("1.2346"));
            QCOMPARE(osb_number(2.0), QString("2"));
            QCOMPARE(osb_number(-0.00001), QString("0"));
            QCOMPARE(osb_number(std::nan("")), QString("0"));
            QCOMPARE(osb_number(1e20), QString("100000000000000000000"));
        }

        void rounds_and_clamps_colours(){
            sprite s;
            s.path = "a.png";
            command colour = make_command(cmd_t::color, 0, 0);
            colour.r0 = colour.r1 = 300;
            colour.g0 = colour.g1 = -5;
            colour.b0 = colour.b1 = 127.6;
            s.cmds << colour;

            QCOMPARE(sprite_lines(s).at(1), QString(" C,0,0,0,255,0,128"));
        }

        void uses_osu_spelling(){
            QCOMPARE(osb_origin_name(origin_t::top_left), QString("TopLeft"));
            QCOMPARE(osb_origin_name(origin_t::top_center), QString("TopCentre"));
            QCOMPARE(osb_origin_name(origin_t::top_right), QString("TopRight"));
            QCOMPARE(osb_origin_name(origin_t::center_left), QString("CentreLeft"));
            QCOMPARE(osb_origin_name(origin_t::center), QString("Centre"));
            QCOMPARE(osb_origin_name(origin_t::center_right), QString("CentreRight"));
            QCOMPARE(osb_origin_name(origin_t::bottom_left), QString("BottomLeft"));
            QCOMPARE(osb_origin_name(origin_t::bottom_center), QString("BottomCentre"));
            QCOMPARE(osb_origin_name(origin_t::bottom_right), QString("BottomRight"));

            QCOMPARE(osb_layer_name(layer_t::background), QString("Background"));
            QCOMPARE(osb_layer_name(layer_t::fail), QString("Fail"));
            QCOMPARE(osb_layer_name(layer_t::pass), QString("Pass"));
            QCOMPARE(osb_layer_name(layer_t::foreground), QString("Foreground"));
            QCOMPARE(osb_layer_name(layer_t::overlay), QString("Overlay"));
        }

        void writes_osu_easing_numbers(){
            sprite s;
            s.path = "a.png";
            s.cmds << make_command(cmd_t::fade, 0, 100, 0, 1, easing_t::bounce_out)
                   << make_command(cmd_t::scale, 0, 100, 1, 2, easing_t::elastic_half_out);

            QCOMPARE(sprite_lines(s).mid(1), QStringList({" F,33,0,100,0,1", " S,26,0,100,1,2"}));
        }

        void writes_animations(){
            sprite s = faded_sprite("sb/flame.png");
            s.layer = layer_t::background;
            s.frame_count = 8;
            s.frame_delay = 41.6667;
            QCOMPARE(sprite_lines(s).at(0),
                     QString("Animation,Background,Centre,\"sb/flame.png\",320,240,8,41.6667,LoopForever"));

            s.loop_type = animation_loop_t::loop_once;
            QVERIFY(sprite_lines(s).at(0).endsWith(",8,41.6667,LoopOnce"));
        }

        void warns_about_animation_paths_with_extra_dots(){
            // osu!lazer puts the frame number before every dot.
            scene sc;
            sprite s = faded_sprite("sb/flame.v2.png");
            s.frame_count = 2;
            s.frame_delay = 100;
            s.source_line = 4;
            sc.sprites << s;

            QList<osb_warning> warnings;
            write_osb(sc, &warnings);
            QCOMPARE(warnings.size(), 1);
            QVERIFY(warnings[0].message.contains("exactly one dot"));
            QCOMPARE(warnings[0].line, 4);
        }

        void writes_loops_and_triggers_unexpanded(){
            sprite s = faded_sprite("a.png");

            loop_group loop;
            loop.start = 1000;
            loop.count = 4;
            loop.cmds << make_command(cmd_t::scale, 0, 250, 1, 1.5, easing_t::sine_out)
                      << make_command(cmd_t::scale, 250, 500, 1.5, 1);
            s.loops << loop;

            trigger_group clap;
            clap.trigger = "HitSoundClap";
            clap.start = 0;
            clap.end = 60000;
            clap.cmds << make_command(cmd_t::fade, 0, 100, 1, 0);
            s.triggers << clap;

            trigger_group failing;
            failing.trigger = "Failing";
            failing.start = 500;
            failing.end = 9000;
            failing.group = 2;
            failing.cmds << make_command(cmd_t::move_x, 0, 0, 10, 10);
            s.triggers << failing;

            QCOMPARE(sprite_lines(s).mid(1), QStringList({
                " F,0,0,1000,0,1",
                " L,1000,4",
                "  S,16,0,250,1,1.5",
                "  S,0,250,500,1.5,1",
                " T,HitSoundClap,0,60000",
                "  F,0,0,100,1,0",
                " T,Failing,500,9000,2",
                "  MX,0,0,0,10",
            }));
        }

        void leaves_out_empty_groups_and_warns(){
            scene sc;
            sprite s = faded_sprite("a.png");
            s.source_line = 9;

            loop_group empty_loop;
            empty_loop.start = 100;
            s.loops << empty_loop;

            trigger_group empty_trigger;
            empty_trigger.trigger = "Passing";
            s.triggers << empty_trigger;
            sc.sprites << s;

            QList<osb_warning> warnings;
            const QString text = write_osb(sc, &warnings);

            QVERIFY(!text.contains(" L,"));
            QVERIFY(!text.contains(" T,"));
            QCOMPARE(warnings.size(), 2);
            QVERIFY(warnings[0].message.contains("loop at 100 ms has no commands"));
            QVERIFY(warnings[1].message.contains("Passing trigger has no commands"));
            QCOMPARE(warnings[1].line, 9);
        }

        void warns_when_only_triggers_would_show_a_sprite(){
            scene sc;
            sprite s;
            s.path = "a.png";

            trigger_group trigger;
            trigger.trigger = "HitSoundClap";
            trigger.end = 1000;
            trigger.cmds << make_command(cmd_t::fade, 0, 100, 1, 0);
            s.triggers << trigger;
            sc.sprites << s;

            QList<osb_warning> warnings;
            write_osb(sc, &warnings);
            QCOMPARE(warnings.size(), 1);
            QVERIFY(warnings[0].message.contains("only has trigger commands"));
        }

        void a_loop_alone_is_enough_to_show_a_sprite(){
            scene sc;
            sprite s;
            s.path = "a.png";

            loop_group loop;
            loop.cmds << make_command(cmd_t::fade, 0, 100, 1, 0);
            s.loops << loop;
            sc.sprites << s;

            QList<osb_warning> warnings;
            write_osb(sc, &warnings);
            QVERIFY(warnings.isEmpty());
        }

        void writes_samples_with_osu_layer_numbers(){
            scene sc;
            sc.samples << sample{100, layer_t::background, "sb/a.wav", 100, 0}
                       << sample{200, layer_t::fail, "sb\\b.wav", 50, 0}
                       << sample{300, layer_t::pass, "c.ogg", 0, 0}
                       << sample{400, layer_t::foreground, "d.mp3", 120, 0};

            const QString text = write_osb(sc);
            QVERIFY(text.endsWith(
                "//Storyboard Sound Samples\n"
                "Sample,100,0,\"sb/a.wav\",100\n"
                "Sample,200,1,\"sb/b.wav\",50\n"
                "Sample,300,2,\"c.ogg\",0\n"
                "Sample,400,3,\"d.mp3\",100\n"));
        }

        void warns_about_unsafe_sample_paths(){
            scene sc;
            sc.samples << sample{0, layer_t::foreground, "../x.wav", 100, 12};

            QList<osb_warning> warnings;
            write_osb(sc, &warnings);
            QCOMPARE(warnings.size(), 1);
            QVERIFY(warnings[0].message.startsWith("../x.wav: sound path"));
            QCOMPARE(warnings[0].line, 12);
        }

        void writes_paths_with_forward_slashes(){
            QCOMPARE(sprite_lines(faded_sprite("sb\\stars\\a.png")).at(0),
                     QString("Sprite,Foreground,Centre,\"sb/stars/a.png\",320,240"));
        }

        void warns_without_blocking(){
            scene sc;

            sprite empty;
            empty.path = "empty.png";
            empty.source_line = 3;
            sc.sprites << empty;

            sprite odd = faded_sprite("../outside.png");
            odd.name = "odd";
            odd.source_line = 7;
            odd.cmds << make_command(cmd_t::move_x, 500, 200, 1, 2);
            sc.sprites << odd;

            QList<osb_warning> warnings;
            const QString text = write_osb(sc, &warnings);

            // Still written, best effort.
            QVERIFY(text.contains("Sprite,Foreground,Centre,\"empty.png\",0,0"));
            QVERIFY(text.contains(" MX,0,500,500,1,2"));        // ends before it starts -> instant

            QCOMPARE(warnings.size(), 3);
            QVERIFY(warnings[0].message.startsWith("empty.png: has no commands"));
            QCOMPARE(warnings[0].line, 3);

            QVERIFY(warnings[1].message.startsWith("odd: image path \"../outside.png\""));
            QVERIFY(warnings[2].message.contains("ends (200 ms) before it starts (500 ms)"));
            QCOMPARE(warnings[2].line, 7);
        }
};

class TestAssetPaths : public QObject{
    Q_OBJECT

    private slots:
        void normalizes(){
            QCOMPARE(normalize_asset_path("sb\\star.png"), QString("sb/star.png"));
            QCOMPARE(normalize_asset_path("./sb/star.png"), QString("sb/star.png"));
            QCOMPARE(normalize_asset_path("  star.png "), QString("star.png"));
        }

        void rejects_paths_leaving_the_folder(){
            QVERIFY(is_safe_asset_path("sb/star.png"));
            QVERIFY(is_safe_asset_path("sb\\..star.png"));      // a name, not ".."

            QVERIFY(!is_safe_asset_path(""));
            QVERIFY(!is_safe_asset_path("/etc/passwd"));
            QVERIFY(!is_safe_asset_path("../star.png"));
            QVERIFY(!is_safe_asset_path("sb/../../star.png"));
            QVERIFY(!is_safe_asset_path("sb\\..\\..\\star.png"));
            QVERIFY(!is_safe_asset_path("C:/star.png"));
            QVERIFY(!is_safe_asset_path("C:star.png"));
        }

        void finds_assets_first(){
            QTemporaryDir dir;
            const QString assets = dir.filePath("assets");
            const QString beatmap = dir.filePath("beatmap");

            QVERIFY(write_file(assets + "/sb/a.png", "assets"));
            QVERIFY(write_file(beatmap + "/sb/a.png", "beatmap"));
            QVERIFY(write_file(beatmap + "/bg.jpg", "bg"));

            const QStringList folders = asset_folders(dir.path(), beatmap);
            QCOMPARE(folders, QStringList({assets, beatmap}));

            QCOMPARE(read_file(find_asset(folders, "sb\\a.png")), QByteArray("assets"));
            QCOMPARE(read_file(find_asset(folders, "bg.jpg")), QByteArray("bg"));
            QVERIFY(find_asset(folders, "nope.png").isEmpty());
            QVERIFY(find_asset(folders, "../beatmap/bg.jpg").isEmpty());
            QVERIFY(find_asset(folders, "sb").isEmpty());       // a folder, not a file
        }
};

class TestStoryboardExport : public QObject{
    Q_OBJECT

    private:
        // <tmp>/osu (the beatmap folder in osu!), <tmp>/project/{assets,beatmap}
        struct folders{
            QTemporaryDir root;
            QString osu() const { return root.filePath("osu"); }
            QString project() const { return root.filePath("project"); }
            QString assets() const { return root.filePath("project/assets"); }
            QString copy() const { return root.filePath("project/beatmap"); }

            folders(){
                QDir().mkpath(osu());
                QDir().mkpath(assets());
                QDir().mkpath(copy());
            }
        };

        static std::optional<export_plan> plan(const folders& f, const scene& sc, QString* error = nullptr){
            return plan_export(sc, f.osu(), "Artist - Song (Me).osb", f.project(), f.assets(), f.copy(), error);
        }

        static scene scene_using(const QStringList& paths){
            scene sc;

            for(const QString& path : paths){
                sc.sprites << faded_sprite(path);
            }

            return sc;
        }

    private slots:
        void names_the_osb_after_the_map(){
            QCOMPARE(osb_file_name("/x/Artist - Song (Me) [Insane].osu"), QString("Artist - Song (Me).osb"));
            QCOMPARE(osb_file_name("A [Remix] - B (C) [Hard].osu"), QString("A [Remix] - B (C).osb"));
            QCOMPARE(osb_file_name("v1.2 map.osu"), QString("v1.2 map.osb"));
        }

        void copies_only_what_the_beatmap_folder_needs(){
            folders f;

            // new: only in assets/
            QVERIFY(write_file(f.assets() + "/sb/new.png", "new"));
            // same: identical in assets/ and in osu!
            QVERIFY(write_file(f.assets() + "/sb/same.png", "same"));
            QVERIFY(write_file(f.osu() + "/sb/same.png", "same"));
            // changed: assets/ has a newer version
            QVERIFY(write_file(f.assets() + "/sb/changed.png", "v2"));
            QVERIFY(write_file(f.osu() + "/sb/changed.png", "v1"));
            // own: the beatmap's file; the older copy never replaces it
            QVERIFY(write_file(f.copy() + "/bg.jpg", "old"));
            QVERIFY(write_file(f.osu() + "/bg.jpg", "current"));
            // lost: only the copy still has it
            QVERIFY(write_file(f.copy() + "/sb/lost.png", "lost"));

            const auto p = plan(f, scene_using({"sb/new.png", "sb/same.png", "sb\\changed.png", "bg.jpg",
                                                "sb/lost.png", "sb/missing.png", "sb/new.png", "../evil.png"}));
            QVERIFY(p.has_value());

            QCOMPARE(p->copies.size(), 3);
            QCOMPARE(p->copies[0].path, QString("sb/new.png"));
            QVERIFY(!p->copies[0].replaces);
            QCOMPARE(p->copies[1].path, QString("sb/changed.png"));
            QVERIFY(p->copies[1].replaces);
            QCOMPARE(p->copies[2].path, QString("sb/lost.png"));
            QVERIFY(!p->copies[2].replaces);

            QCOMPARE(p->missing, QStringList({"sb/missing.png"}));
            QVERIFY(p->replaces_files());
        }

        void notices_existing_storyboards(){
            folders f;
            const auto fresh = plan(f, scene_using({}));
            QVERIFY(fresh.has_value());
            QVERIFY(!fresh->osb_replaces);
            QVERIFY(!fresh->replaces_files());
            QCOMPARE(fresh->osb_path, QDir(f.osu()).filePath("Artist - Song (Me).osb"));

            QVERIFY(write_file(f.osu() + "/Artist - Song (Me).osb", "old"));
            QVERIFY(write_file(f.osu() + "/Other.osb", "other"));

            const auto again = plan(f, scene_using({}));
            QVERIFY(again->osb_replaces);
            QVERIFY(again->replaces_files());
            QCOMPARE(again->other_osb_files, QStringList({"Other.osb"}));
        }

        void refuses_bad_folders(){
            folders f;
            QString error;

            QVERIFY(!plan_export({}, f.root.filePath("gone"), "a.osb", f.project(), f.assets(), f.copy(), &error));
            QVERIFY(error.contains("does not exist"));

            // The project's own beatmap copy is not osu!'s folder.
            QVERIFY(!plan_export({}, f.copy(), "a.osb", f.project(), f.assets(), f.copy(), &error));
            QVERIFY(error.contains("not into the project"));

            QVERIFY(!plan_export({}, f.osu(), "../a.osb", f.project(), f.assets(), f.copy(), &error));
        }

        void lists_every_used_file(){
            scene sc = scene_using({"bg.jpg"});

            sprite flame = faded_sprite("sb/flame.png");
            flame.frame_count = 3;
            flame.frame_delay = 50;
            sc.sprites << flame;
            sc.samples << sample{0, layer_t::foreground, "sb/hit.wav", 100, 0};

            QCOMPARE(used_files(sc), QStringList({"bg.jpg", "sb/flame0.png", "sb/flame1.png",
                                                  "sb/flame2.png", "sb/hit.wav"}));
        }

        void copies_animation_frames_and_sounds(){
            folders f;
            QVERIFY(write_file(f.assets() + "/sb/flame0.png", "0"));
            QVERIFY(write_file(f.assets() + "/sb/flame1.png", "1"));
            QVERIFY(write_file(f.assets() + "/sb/hit.wav", "hit"));

            scene sc;
            sprite flame = faded_sprite("sb/flame.png");
            flame.frame_count = 3;
            flame.frame_delay = 50;
            sc.sprites << flame;
            sc.samples << sample{0, layer_t::foreground, "sb/hit.wav", 100, 0};

            const auto p = plan(f, sc);
            QVERIFY(p.has_value());
            QCOMPARE(p->copies.size(), 3);
            QCOMPARE(p->copies[0].path, QString("sb/flame0.png"));
            QCOMPARE(p->copies[1].path, QString("sb/flame1.png"));
            QCOMPARE(p->copies[2].path, QString("sb/hit.wav"));
            QCOMPARE(p->missing, QStringList({"sb/flame2.png"}));
        }

        void writes_the_osb_and_images(){
            folders f;
            QVERIFY(write_file(f.assets() + "/sb/deep/a.png", "image"));
            QVERIFY(write_file(f.osu() + "/Artist - Song (Me).osb", "old"));

            const scene sc = scene_using({"sb/deep/a.png"});
            const auto p = plan(f, sc);
            QVERIFY(p.has_value());

            QString error;
            QVERIFY2(apply_export(*p, write_osb(sc), &error), qPrintable(error));

            QCOMPARE(read_file(f.osu() + "/sb/deep/a.png"), QByteArray("image"));
            QCOMPARE(read_file(p->osb_path), write_osb(sc).toUtf8());

            // Nothing left to copy the second time.
            QVERIFY(plan(f, sc)->copies.isEmpty());
        }
};

class TestAssetCheck : public QObject{
    Q_OBJECT

    private slots:
        void reports_each_missing_file_once_at_its_first_line(){
            QTemporaryDir dir;
            QVERIFY(write_file(dir.filePath("here.png"), "x"));

            scene sc;
            sprite here = faded_sprite("here.png");
            sprite gone = faded_sprite("sb\\gone.png");
            gone.source_line = 5;
            sprite gone_again = faded_sprite("sb/gone.png");
            gone_again.source_line = 8;
            sprite unsafe = faded_sprite("/abs/x.png");
            unsafe.source_line = 9;
            sc.sprites << here << gone << gone_again << unsafe;

            sc.samples << sample{0, layer_t::foreground, "boom.wav", 100, 11};

            const auto problems = check_assets(sc, {dir.path()});
            QCOMPARE(problems.size(), 3);
            QCOMPARE(problems[0].message, QString("sb/gone.png: image not found in assets/ or beatmap/"));
            QCOMPARE(problems[0].source_line, 5);
            QVERIFY(problems[1].message.contains("must be relative"));
            QCOMPARE(problems[1].source_line, 9);
            QCOMPARE(problems[2].message, QString("boom.wav: sound not found in assets/ or beatmap/"));
            QCOMPARE(problems[2].source_line, 11);
        }

        void groups_missing_animation_frames(){
            QTemporaryDir dir;
            QVERIFY(write_file(dir.filePath("f0.png"), "x"));
            QVERIFY(write_file(dir.filePath("f2.png"), "x"));

            scene sc;
            sprite some = faded_sprite("f.png");
            some.frame_count = 4;
            some.frame_delay = 10;
            sprite none = faded_sprite("g.png");
            none.frame_count = 3;
            none.frame_delay = 10;
            sc.sprites << some << none;

            const auto problems = check_assets(sc, {dir.path()});
            QCOMPARE(problems.size(), 2);
            QCOMPARE(problems[0].message, QString("f.png: 2 of 4 animation frames not found: f1.png, f3.png"));
            QCOMPARE(problems[1].message,
                     QString("g.png: none of the 3 animation frames found (g0.png ... g2.png) in assets/ or beatmap/"));
        }

        void suggests_a_similar_file_next_to_a_missing_one(){
            QTemporaryDir assets;
            QTemporaryDir beatmap;
            QVERIFY(write_file(assets.filePath("sb/Star.png"), "x"));
            QVERIFY(write_file(assets.filePath("sb/glow.jpg"), "x"));
            QVERIFY(write_file(beatmap.filePath("sb/flare.png"), "x"));
            QVERIFY(write_file(beatmap.filePath("Hit.wav"), "x"));

            scene sc;
            sc.sprites << faded_sprite("sb/star.png")      // another case
                       << faded_sprite("sb/glow.png")      // another extension
                       << faded_sprite("sb/flair.png")     // a typo, in the other folder
                       << faded_sprite("sb/moon.png")      // nothing like it
                       << faded_sprite("fx/star.png");     // a folder that doesn't exist
            sc.samples << sample{0, layer_t::foreground, "hit.wav", 100, 3};

            const auto problems = check_assets(sc, {assets.path(), beatmap.path()});
            QCOMPARE(problems.size(), 6);
            QCOMPARE(problems[0].message,
                     QString("sb/star.png: image not found in assets/ or beatmap/. Did you mean \"sb/Star.png\"?"));
            QCOMPARE(problems[1].message,
                     QString("sb/glow.png: image not found in assets/ or beatmap/. Did you mean \"sb/glow.jpg\"?"));
            QCOMPARE(problems[2].message,
                     QString("sb/flair.png: image not found in assets/ or beatmap/. Did you mean \"sb/flare.png\"?"));
            QCOMPARE(problems[3].message, QString("sb/moon.png: image not found in assets/ or beatmap/"));
            QCOMPARE(problems[4].message, QString("fx/star.png: image not found in assets/ or beatmap/"));
            QCOMPARE(problems[5].message,
                     QString("hit.wav: sound not found in assets/ or beatmap/. Did you mean \"Hit.wav\"?"));
        }

        void everything_found_means_no_problems(){
            QTemporaryDir dir;
            QVERIFY(write_file(dir.filePath("a.png"), "x"));
            QVERIFY(check_assets(scene_with("a.png"), {dir.path()}).isEmpty());
            QVERIFY(check_assets(scene{}, {}).isEmpty());
        }

    private:
        static scene scene_with(const QString& path){
            scene sc;
            sc.sprites << faded_sprite(path);
            return sc;
        }
};

COSBY_TEST(TestOsbWriter)
COSBY_TEST(TestAssetCheck)
COSBY_TEST(TestAssetPaths)
COSBY_TEST(TestStoryboardExport)
#include "test_osb_export.moc"
