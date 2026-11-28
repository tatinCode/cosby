#include "core/easing.h"
#include "cosby_test.h"

#include <QTest>

#include <cmath>

namespace {
    bool near(double a, double b, double tolerance = 1e-9){
        return std::abs(a - b) <= tolerance;
    }

    QByteArray describe(easing_t easing, double t, double got, double expected){
        return QStringLiteral("%1(%2) = %3, expected %4")
            .arg(easing_name(easing)).arg(t).arg(got, 0, 'g', 12).arg(expected, 0, 'g', 12).toUtf8();
    }
}

class TestEasing : public QObject{
    Q_OBJECT

    private slots:
        void names_follow_osu_numbering(){
            QCOMPARE(easing_names().size(), qsizetype(35));
            QCOMPARE(easing_name(easing_t(0)), QString("Linear"));
            QCOMPARE(easing_name(easing_t(1)), QString("Out"));
            QCOMPARE(easing_name(easing_t(2)), QString("In"));
            QCOMPARE(easing_name(easing_t(4)), QString("QuadOut"));
            QCOMPARE(easing_name(easing_t(17)), QString("SineInOut"));
            QCOMPARE(easing_name(easing_t(26)), QString("ElasticHalfOut"));
            QCOMPARE(easing_name(easing_t(27)), QString("ElasticQuarterOut"));
            QCOMPARE(easing_name(easing_t(28)), QString("ElasticInOut"));
            QCOMPARE(easing_name(easing_t(34)), QString("BounceInOut"));
        }

        void parses_names_in_any_case_and_numbers(){
            easing_t easing = easing_t::linear;

            QVERIFY(parse_easing("quadout", easing));
            QCOMPARE(easing, easing_t::quad_out);
            QVERIFY(parse_easing("  BOUNCEINOUT ", easing));
            QCOMPARE(easing, easing_t::bounce_in_out);
            QVERIFY(parse_easing("33", easing));
            QCOMPARE(easing, easing_t::bounce_out);
            QVERIFY(parse_easing("0", easing));
            QCOMPARE(easing, easing_t::linear);

            // Every name comes back as itself.
            for(int i = 0; i < easing_count; ++i){
                QVERIFY(parse_easing(easing_name(easing_t(i)), easing));
                QCOMPARE(int(easing), i);
            }
        }

        void rejects_unknown_easings_without_touching_the_result(){
            easing_t easing = easing_t::sine_in;

            for(const char* bad : {"Wobbly", "", "35", "-1", "1.5", "Quad"}){
                QVERIFY2(!parse_easing(bad, easing), bad);
                QCOMPARE(easing, easing_t::sine_in);
            }
        }

        void every_easing_starts_at_0_and_ends_at_1(){
            for(int i = 0; i < easing_count; ++i){
                const auto easing = easing_t(i);
                QCOMPARE(apply_easing(easing, 0.0), 0.0);
                QCOMPARE(apply_easing(easing, 1.0), 1.0);

                // Outside [0, 1] is clamped.
                QCOMPARE(apply_easing(easing, -0.5), 0.0);
                QCOMPARE(apply_easing(easing, 1.5), 1.0);
            }
        }

        void every_easing_is_continuous_at_its_ends(){
            // osu!'s offsets make expo and elastic curves meet 0 and 1
            // exactly; a wrong constant shows up as a jump of about 0.001.
            // (Circ is steep at its ends: sqrt(2e-12) is about 1.4e-6.)
            constexpr double tiny = 1e-12;

            for(int i = 0; i < easing_count; ++i){
                const auto easing = easing_t(i);
                const double start = apply_easing(easing, tiny);
                const double end = apply_easing(easing, 1 - tiny);

                QVERIFY2(near(start, 0.0, 1e-5), describe(easing, tiny, start, 0.0));
                QVERIFY2(near(end, 1.0, 1e-5), describe(easing, 1 - tiny, end, 1.0));
            }
        }

        void in_out_variants_are_continuous_in_the_middle(){
            for(easing_t easing : {easing_t::quad_in_out, easing_t::cubic_in_out, easing_t::quart_in_out,
                                   easing_t::quint_in_out, easing_t::sine_in_out, easing_t::expo_in_out,
                                   easing_t::circ_in_out, easing_t::elastic_in_out, easing_t::back_in_out,
                                   easing_t::bounce_in_out}){
                const double below = apply_easing(easing, 0.5 - 1e-12);
                const double above = apply_easing(easing, 0.5 + 1e-12);

                QVERIFY2(near(below, 0.5, 1e-5), describe(easing, 0.5, below, 0.5));
                QVERIFY2(near(above, 0.5, 1e-5), describe(easing, 0.5, above, 0.5));
            }
        }

        void in_is_out_mirrored(){
            // X-In(t) == 1 - X-Out(1 - t) for each family.
            const std::pair<easing_t, easing_t> pairs[] = {
                {easing_t::in, easing_t::out},
                {easing_t::quad_in, easing_t::quad_out},
                {easing_t::cubic_in, easing_t::cubic_out},
                {easing_t::quart_in, easing_t::quart_out},
                {easing_t::quint_in, easing_t::quint_out},
                {easing_t::sine_in, easing_t::sine_out},
                {easing_t::expo_in, easing_t::expo_out},
                {easing_t::circ_in, easing_t::circ_out},
                {easing_t::elastic_in, easing_t::elastic_out},
                {easing_t::back_in, easing_t::back_out},
                {easing_t::bounce_in, easing_t::bounce_out},
            };

            for(const auto& [in, out] : pairs){
                for(double t = 0.05; t < 1.0; t += 0.05){
                    const double got = apply_easing(in, t);
                    const double expected = 1 - apply_easing(out, 1 - t);
                    QVERIFY2(near(got, expected), describe(in, t, got, expected));
                }
            }
        }

        void known_values(){
            QCOMPARE(apply_easing(easing_t::linear, 0.25), 0.25);
            QCOMPARE(apply_easing(easing_t::in, 0.5), 0.25);
            QCOMPARE(apply_easing(easing_t::out, 0.5), 0.75);
            QCOMPARE(apply_easing(easing_t::quad_out, 0.5), 0.75);
            QCOMPARE(apply_easing(easing_t::cubic_in, 0.5), 0.125);
            QCOMPARE(apply_easing(easing_t::quart_out, 0.5), 0.9375);
            QCOMPARE(apply_easing(easing_t::quint_in, 0.5), 0.03125);
            QVERIFY(near(apply_easing(easing_t::circ_in, 0.6), 0.2));
            QVERIFY(near(apply_easing(easing_t::sine_out, 1.0 / 3.0), 0.5));

            // Bounce: touches 1 at the end of the first fall (t = 1/2.75).
            QVERIFY(near(apply_easing(easing_t::bounce_out, 1 / 2.75), 1.0));
        }

        void back_and_elastic_overshoot(){
            // Back pulls below 0 first, Elastic swings past 1: osu! keeps that.
            QVERIFY(apply_easing(easing_t::back_in, 0.3) < 0.0);
            QVERIFY(apply_easing(easing_t::back_out, 0.7) > 1.0);
            QVERIFY(apply_easing(easing_t::elastic_out, 0.1) > 1.0);

            // Back-In's lowest point is about -0.1 (osu!'s 1.70158 constant).
            double lowest = 1.0;
            for(double t = 0.0; t <= 1.0; t += 0.001){
                lowest = std::min(lowest, apply_easing(easing_t::back_in, t));
            }
            QVERIFY(near(lowest, -0.1, 0.001));
        }
};

COSBY_TEST(TestEasing)
#include "test_easing.moc"
