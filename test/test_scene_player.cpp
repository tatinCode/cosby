#include "core/scene_player.h"
#include "cosby_test.h"

#include <QImage>
#include <QTest>


class TestScenePlayer : public QObject{
    Q_OBJECT

    private slots:
        // Test cases for ScenePlayer
        void test_ease_linear();
        void test_ease_in();
        void test_ease_out();
        void test_eval_sprite_fade();
        void test_eval_sprite_move();
        void test_draw_uses_layer_order();
        void test_draw_skips_hidden_layers();
        void test_visible_only_during_commands();
        void test_start_value_before_first_command();
        void test_latest_started_command_wins();
        void test_move_x_and_move_share_x();
        void test_permanent_and_timed_flags();
        void test_color_tints_images();
};

namespace {
    command fade(int t0, int t1, double v0, double v1){
        command cmd;
        cmd.type = cmd_t::fade;
        cmd.easing = "Linear";
        cmd.t0 = t0;
        cmd.t1 = t1;
        cmd.v0 = v0;
        cmd.v1 = v1;
        return cmd;
    }

    command make_scale(int t0, int t1, double v0, double v1){
        command cmd = fade(t0, t1, v0, v1);
        cmd.type = cmd_t::scale;
        return cmd;
    }

    // A 64x64 foreground sprite created *before* a background one, both
    // covering the middle of a 128x128 image.
    QImage render(unsigned visible_layers){
        scene sc;
        sc.add_sprite("front.png", layer_t::foreground, origin_t::center, 64, 64);
        sc.add_sprite("back.png", layer_t::background, origin_t::center, 64, 64);

        // osu! only shows sprites that have commands.
        for(auto& s : sc.sprites){
            s.cmds.push_back(fade(0, 1000, 1.0, 1.0));
        }

        QPixmap front(64, 64);
        front.fill(Qt::blue);
        QPixmap back(64, 64);
        back.fill(Qt::red);

        const QHash<QString, QPixmap> pixmaps{{"front.png", front}, {"back.png", back}};

        QImage image(128, 128, QImage::Format_ARGB32);
        image.fill(Qt::black);

        QPainter p(&image);
        draw_scene(p, sc, 0, pixmaps, visible_layers);
        p.end();

        return image;
    }
}

void TestScenePlayer::test_draw_uses_layer_order(){
    // Background is drawn first even though it was created last.
    QCOMPARE(QColor(render(all_layers_mask).pixel(64, 64)), QColor(Qt::blue));
}

void TestScenePlayer::test_draw_skips_hidden_layers(){
    const QImage image = render(all_layers_mask & ~layer_bit(layer_t::foreground));
    QCOMPARE(QColor(image.pixel(64, 64)), QColor(Qt::red));

    const QImage none = render(0);
    QCOMPARE(QColor(none.pixel(64, 64)), QColor(Qt::black));
}

void TestScenePlayer::test_ease_linear(){
    QCOMPARE(ease("Linear", 0.0), 0.0);
    QCOMPARE(ease("Linear", 0.5), 0.5);
    QCOMPARE(ease("Linear", 1.0), 1.0);
}

void TestScenePlayer::test_ease_in(){
    //ease("In", t) = t*t   (quadratic ease-in)
    QCOMPARE(ease("In", 0.0), 0.0);
    QCOMPARE(ease("In", 0.5), 0.25); // 0.5 * 0.5 = 0.25
    QCOMPARE(ease("In", 1.0), 1.0);
}

void TestScenePlayer::test_ease_out(){
    //ease("Out", t) = 1 - (1 - t)*(1 - t)   (quadratic ease-out)
    QCOMPARE(ease("Out", 0.0), 0.0);
    QCOMPARE(ease("Out", 0.5), 0.75); // 1 - (1 - 0.5)*(1 - 0.5) = 1 - 0.25 = 0.75
    QCOMPARE(ease("Out", 1.0), 1.0);
}

void TestScenePlayer::test_eval_sprite_fade(){
    sprite s;

    s.x = 320;
    s.y = 240;

    command cmd;

    cmd.type = cmd_t::fade;
    cmd.easing = "Linear";

    cmd.t0 = 0;
    cmd.t1 = 1000;
    
    cmd.v0 = 0.0;   //start fully transparent
    cmd.v1 = 1.0;   //end fully opaque

    s.cmds.push_back(cmd);

    // Test at t = 0 (start), alpha should be 0.0
    auto state = eval_sprite(s, 0);
    QCOMPARE(state.alpha, 0.0);
    
    // Test at t = 500 (midway), alpha should be 0.5
    state = eval_sprite(s, 500);
    QCOMPARE(state.alpha, 0.5);

    // Test at t = 1000 (midway), alpha should be 1.0
    state = eval_sprite(s, 1000);
    QCOMPARE(state.alpha, 1.0);
}


void TestScenePlayer::test_eval_sprite_move(){
    sprite s;

    s.x = 0;
    s.y = 0;

    command cmd;

    cmd.type = cmd_t::move;
    cmd.easing = "Linear";

    cmd.t0 = 0;
    cmd.t1 = 1000;

    cmd.x0 = 100;   //start x
    cmd.y0 = 100;   //start y

    cmd.x1 = 200;   //end x
    cmd.y1 = 300;   //end y
    
    s.cmds.push_back(cmd);

    // Test at t = 0 (start), alpha should be 0.0
    auto state = eval_sprite(s, 0);
    QCOMPARE(state.x, 100.0);
    QCOMPARE(state.y, 100.0);

    // Test at t = 500 (midway), alpha should
    state = eval_sprite(s, 500);
    QCOMPARE(state.x, 150.0);
    QCOMPARE(state.y, 200.0);

    // Test at t = 1000 (midway), alpha should be 1.0
    state = eval_sprite(s, 1000);
    QCOMPARE(state.x, 200.0);
    QCOMPARE(state.y, 300.0);
}

void TestScenePlayer::test_visible_only_during_commands(){
    sprite s;
    QVERIFY(!eval_sprite(s, 0).visible);       // no commands: never shown

    s.cmds.push_back(fade(1000, 2000, 1.0, 1.0));
    s.cmds.push_back(fade(3000, 4000, 1.0, 0.0));

    QVERIFY(!eval_sprite(s, 999).visible);
    QVERIFY(eval_sprite(s, 1000).visible);
    QVERIFY(eval_sprite(s, 2500).visible);     // gap between commands
    QVERIFY(eval_sprite(s, 4000).visible);
    QVERIFY(!eval_sprite(s, 4001).visible);
}

void TestScenePlayer::test_start_value_before_first_command(){
    sprite s;
    s.cmds.push_back(fade(0, 100, 1.0, 1.0));
    s.cmds.push_back(make_scale(500, 1000, 0.25, 2.0));

    // The sprite exists from 0; its scale already is the first scale's start.
    QCOMPARE(eval_sprite(s, 0).scale, 0.25);
    QCOMPARE(eval_sprite(s, 750).scale, 1.125);
    QCOMPARE(eval_sprite(s, 1000).scale, 2.0);
}

void TestScenePlayer::test_latest_started_command_wins(){
    sprite s;
    s.cmds.push_back(fade(0, 1000, 0.0, 1.0));
    s.cmds.push_back(fade(2000, 3000, 0.5, 0.0));

    QCOMPARE(eval_sprite(s, 1500).alpha, 1.0);    // first holds its end value
    QCOMPARE(eval_sprite(s, 2000).alpha, 0.5);
    QCOMPARE(eval_sprite(s, 2500).alpha, 0.25);
}

void TestScenePlayer::test_move_x_and_move_share_x(){
    sprite s;
    s.x = 10;
    s.y = 20;

    command move;
    move.type = cmd_t::move;
    move.easing = "Linear";
    move.t0 = 0;
    move.t1 = 1000;
    move.x0 = 0;
    move.y0 = 0;
    move.x1 = 100;
    move.y1 = 200;
    s.cmds.push_back(move);

    command move_x;
    move_x.type = cmd_t::move_x;
    move_x.easing = "Linear";
    move_x.t0 = 500;
    move_x.t1 = 500;
    move_x.v0 = move_x.v1 = 400;
    s.cmds.push_back(move_x);

    const auto before = eval_sprite(s, 250);
    QCOMPARE(before.x, 25.0);
    QCOMPARE(before.y, 50.0);

    const auto after = eval_sprite(s, 750);
    QCOMPARE(after.x, 400.0);                   // moveX took over x
    QCOMPARE(after.y, 150.0);                   // y still follows move
}

void TestScenePlayer::test_permanent_and_timed_flags(){
    sprite s;
    s.cmds.push_back(fade(0, 3000, 1.0, 1.0));

    command flip;
    flip.type = cmd_t::flip_h;
    flip.t0 = 1000;
    flip.t1 = 1000;                 // permanent
    s.cmds.push_back(flip);

    command add;
    add.type = cmd_t::additive;
    add.t0 = 1000;
    add.t1 = 2000;                  // only while it runs
    s.cmds.push_back(add);

    QVERIFY(!eval_sprite(s, 999).flip_h);
    QVERIFY(eval_sprite(s, 2500).flip_h);
    QVERIFY(eval_sprite(s, 1500).additive);
    QVERIFY(!eval_sprite(s, 2000).additive);
}

void TestScenePlayer::test_color_tints_images(){
    scene sc;
    sc.add_sprite("white.png", layer_t::foreground, origin_t::center, 16, 16);

    command colour;
    colour.type = cmd_t::color;
    colour.easing = "Linear";
    colour.t0 = 0;
    colour.t1 = 1000;
    colour.r0 = colour.r1 = 255;
    colour.g0 = colour.g1 = 0;
    colour.b0 = colour.b1 = 0;
    sc.sprites[0].cmds.push_back(colour);

    QPixmap white(32, 32);
    white.fill(Qt::white);
    const QHash<QString, QPixmap> pixmaps{{"white.png", white}};

    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(Qt::black);

    QPainter p(&image);
    draw_scene(p, sc, 500, pixmaps, all_layers_mask);
    p.end();

    QCOMPARE(QColor(image.pixel(16, 16)), QColor(Qt::red));
}

COSBY_TEST(TestScenePlayer)
#include "test_scene_player.moc"
