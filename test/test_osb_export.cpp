#include "core/osb_writer.h"
#include "project/asset_paths.h"
#include "project/storyboard_export.h"
#include "cosby_test.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>

namespace {
    command make_command(cmd_t type, int t0, int t1, double v0 = 0.0, double v1 = 0.0, QString easing = "Linear"){
        command cmd;
        cmd.type = type;
        cmd.easing = std::move(easing);
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
            command move = make_command(cmd_t::move, 0, 500, 0, 0, "Out");
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

            s.cmds << make_command(cmd_t::scale, 0, 100, 1, 2, "In")
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
            odd.cmds << make_command(cmd_t::fade, 0, 100, 0, 1, "Wobbly")
                     << make_command(cmd_t::scale, 0, 100, 1, 2, "Wobbly")
                     << make_command(cmd_t::move_x, 500, 200, 1, 2);
            sc.sprites << odd;

            QList<osb_warning> warnings;
            const QString text = write_osb(sc, &warnings);

            // Still written, best effort.
            QVERIFY(text.contains("Sprite,Foreground,Centre,\"empty.png\",0,0"));
            QVERIFY(text.contains(" F,0,0,100,0,1"));           // unknown easing -> Linear
            QVERIFY(text.contains(" MX,0,500,500,1,2"));        // ends before it starts -> instant

            QCOMPARE(warnings.size(), 4);
            QVERIFY(warnings[0].message.startsWith("empty.png: has no commands"));
            QCOMPARE(warnings[0].line, 3);

            QVERIFY(warnings[1].message.startsWith("odd: image path \"../outside.png\""));
            QVERIFY(warnings[2].message.contains("unknown easing \"Wobbly\""));     // once, not twice
            QVERIFY(warnings[3].message.contains("ends (200 ms) before it starts (500 ms)"));
            QCOMPARE(warnings[3].line, 7);
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

COSBY_TEST(TestOsbWriter)
COSBY_TEST(TestAssetPaths)
COSBY_TEST(TestStoryboardExport)
#include "test_osb_export.moc"
