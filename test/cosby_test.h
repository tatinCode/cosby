#pragma once

// All test classes run from one program (cosby_tests, see test_main.cpp).
// Each test file ends with COSBY_TEST(ClassName) instead of QTEST_MAIN.

#include <QObject>

#include <functional>
#include <memory>
#include <vector>

namespace cosby_test {
    struct entry {
        const char* name;
        std::function<std::unique_ptr<QObject>()> make;
    };

    inline std::vector<entry>& registry(){
        static std::vector<entry> tests;
        return tests;
    }

    struct registrar {
        registrar(const char* name, std::function<std::unique_ptr<QObject>()> make){
            registry().push_back({name, std::move(make)});
        }
    };
}

#define COSBY_TEST(Class) \
    static const cosby_test::registrar Class##_registrar(#Class, []{ \
        return std::unique_ptr<QObject>(new Class); \
    });
