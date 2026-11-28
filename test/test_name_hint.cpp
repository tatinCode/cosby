#include "core/name_hint.h"
#include "cosby_test.h"

#include <QtTest>

class TestNameHint : public QObject {
    Q_OBJECT

    private slots:
        void suggests_the_closest_name(){
            const QStringList layers{"Background", "Fail", "Pass", "Foreground", "Overlay"};

            QCOMPARE(closest_name("Foregroud", layers), QString("Foreground"));
            QCOMPARE(closest_name("backround", layers), QString("Background"));
            QCOMPARE(closest_name("Pas", layers), QString("Pass"));
        }

        void swapped_letters_count_as_one_change(){
            QCOMPARE(closest_name("Fial", {"Fail"}), QString("Fail"));
            QCOMPARE(closest_name("Foregorund", {"Background", "Foreground"}), QString("Foreground"));
            QCOMPARE(closest_name("strat", {"start"}), QString("start"));
        }

        void ignores_case(){
            QCOMPARE(closest_name("FOREGROUND", {"Background", "Foreground"}), QString("Foreground"));
            QCOMPARE(closest_name("star.PNG", {"Star.png"}), QString("Star.png"));
        }

        void suggests_nothing_when_nothing_is_close(){
            const QStringList layers{"Background", "Fail", "Pass", "Foreground", "Overlay"};

            QVERIFY(closest_name("xyz", layers).isEmpty());
            QVERIFY(closest_name("", layers).isEmpty());
            QVERIFY(closest_name("Foreground", {}).isEmpty());
            // A third of the length may differ: 1 change for 4 letters, not 2.
            QCOMPARE(closest_name("Fai", {"Fail"}), QString("Fail"));
            QVERIFY(closest_name("Fxyl", {"Fail"}).isEmpty());
        }

        void ties_go_to_the_earlier_candidate(){
            QCOMPARE(closest_name("Pa", {"Pass", "Paxx", "Pa1"}), QString("Pa1"));
            QCOMPARE(closest_name("ab", {"ax", "ay"}), QString("ax"));
        }

        void did_you_mean_reads_as_a_sentence(){
            QCOMPARE(did_you_mean("startTim", {"startTime", "endTime"}), QString(" Did you mean \"startTime\"?"));
            QVERIFY(did_you_mean("zzz", {"startTime"}).isEmpty());
        }
};

COSBY_TEST(TestNameHint)
#include "test_name_hint.moc"
