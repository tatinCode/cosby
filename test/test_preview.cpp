#include "ui/preview_widget.h"
#include "cosby_test.h"

#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <memory>

// The preview's stand-in background: the beatmap's image, dimmed, while the
// storyboard has no sprites.

namespace {
    const QColor image_color(200, 100, 50);

    // What image_color looks like after the dim.
    QColor dimmed(const QColor& c){
        const double keep = 1.0 - PreviewWidget::placeholder_dim;
        return QColor(int(std::lround(c.red() * keep)), int(std::lround(c.green() * keep)),
                      int(std::lround(c.blue() * keep)));
    }

    bool close_to(const QColor& a, const QColor& b){
        return std::abs(a.red() - b.red()) <= 2 && std::abs(a.green() - b.green()) <= 2
            && std::abs(a.blue() - b.blue()) <= 2;
    }

    // A sprite that exists but isn't on screen at time 0.
    scene scene_with_a_later_sprite(){
        scene sc;
        const qsizetype i = sc.add_sprite("later.png", layer_t::foreground, origin_t::center, 320, 240);

        command fade;
        fade.type = cmd_t::fade;
        fade.t0 = 10000;
        fade.t1 = 20000;
        fade.v0 = 1;
        fade.v1 = 1;
        sc.sprites[i].cmds << fade;

        return sc;
    }
}

class TestPreviewWidget : public QObject{
    Q_OBJECT

    private:
        QTemporaryDir m_dir;
        QString m_image;

        std::unique_ptr<PreviewWidget> make_preview(){
            auto preview = std::make_unique<PreviewWidget>();
            preview->resize(854, 480);      // the screen is the whole widget
            return preview;
        }

        static QColor pixel(PreviewWidget& preview, int x, int y){
            return preview.grab().toImage().pixelColor(x, y);
        }

    private slots:
        void initTestCase(){
            QVERIFY(m_dir.isValid());

            // 2:1, wider than the 16:9 screen: filling it crops the sides.
            QImage image(200, 100, QImage::Format_RGB32);
            image.fill(image_color);
            m_image = m_dir.filePath("bg.png");
            QVERIFY(image.save(m_image));
        }

        void shows_the_dimmed_background_before_the_script_runs(){
            auto preview = make_preview();
            preview->set_placeholder_background(m_image);
            QVERIFY(preview->has_placeholder_background());

            const QColor expected = dimmed(image_color);
            QVERIFY2(close_to(pixel(*preview, 427, 240), expected),
                     qPrintable(pixel(*preview, 427, 240).name()));

            // It fills the screen (no black bars at the top or the sides).
            QVERIFY(close_to(pixel(*preview, 427, 1), expected));
            QVERIFY(close_to(pixel(*preview, 1, 240), expected));
        }

        void stays_while_the_storyboard_has_no_sprites(){
            auto preview = make_preview();
            preview->set_placeholder_background(m_image);

            const scene empty;
            preview->set_scene(&empty);
            QVERIFY(close_to(pixel(*preview, 427, 240), dimmed(image_color)));
        }

        void goes_away_once_there_is_a_sprite(){
            auto preview = make_preview();
            preview->set_placeholder_background(m_image);

            const scene sc = scene_with_a_later_sprite();
            preview->set_scene(&sc);
            QCOMPARE(pixel(*preview, 427, 240), QColor(Qt::black));

            // Back when the script makes none again.
            preview->set_scene(nullptr);
            QVERIFY(close_to(pixel(*preview, 427, 240), dimmed(image_color)));
        }

        void no_image_or_a_missing_one_leaves_it_black(){
            auto preview = make_preview();
            QCOMPARE(pixel(*preview, 427, 240), QColor(Qt::black));

            preview->set_placeholder_background(m_dir.filePath("missing.png"));
            QVERIFY(!preview->has_placeholder_background());
            QCOMPARE(pixel(*preview, 427, 240), QColor(Qt::black));

            preview->set_placeholder_background(m_image);
            preview->set_placeholder_background({});     // project closed or no background
            QCOMPARE(pixel(*preview, 427, 240), QColor(Qt::black));
        }
};

COSBY_TEST(TestPreviewWidget)
#include "test_preview.moc"
