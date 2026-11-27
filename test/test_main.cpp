#include "cosby_test.h"

#include <QApplication>
#include <QTest>

#include <algorithm>
#include <cstring>

// Runs every registered test class, or only the ones named on the command
// line. Anything after the names goes to QTest (e.g. -v2, or a function name):
//
//   cosby_tests                      all classes
//   cosby_tests TestTimelineView     one class
//   cosby_tests TestTimelineView -v2 one class, verbose
int main(int argc, char* argv[]){
    QApplication app(argc, argv);

    std::vector<cosby_test::entry> tests = cosby_test::registry();
    std::sort(tests.begin(), tests.end(), [](const auto& a, const auto& b){
        return std::strcmp(a.name, b.name) < 0;
    });

    QStringList selected;
    QStringList qtest_args{QString::fromLocal8Bit(argv[0])};

    for(int i = 1; i < argc; ++i){
        const QString arg = QString::fromLocal8Bit(argv[i]);

        if(qtest_args.size() == 1 && arg.startsWith("Test")){
            selected << arg;
        }
        else{
            qtest_args << arg;
        }
    }

    for(const QString& name : selected){
        const bool known = std::any_of(tests.begin(), tests.end(), [&](const auto& t){
            return name == QLatin1String(t.name);
        });

        if(!known){
            qCritical("Unknown test class: %s", qPrintable(name));
            return 2;
        }
    }

    int failed_classes = 0;

    for(const auto& test : tests){
        if(!selected.isEmpty() && !selected.contains(QLatin1String(test.name))){
            continue;
        }

        const std::unique_ptr<QObject> object = test.make();
        if(QTest::qExec(object.get(), qtest_args) != 0){
            ++failed_classes;
        }
    }

    return failed_classes == 0 ? 0 : 1;
}
