#include "core/scene_player.h"

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
};
