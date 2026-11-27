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
};

namespace {
    // A 64x64 foreground sprite created *before* a background one, both
    // covering the middle of a 128x128 image.
    QImage render(unsigned visible_layers){
        scene sc;
        sc.add_sprite("front.png", layer_t::foreground, origin_t::center, 64, 64);
        sc.add_sprite("back.png", layer_t::background, origin_t::center, 64, 64);

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

COSBY_TEST(TestScenePlayer)
#include "test_scene_player.moc"
