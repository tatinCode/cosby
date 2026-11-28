#pragma once

#include <QJSValue>
#include <QString>
#include <QStringList>

// Checks script values strictly: text, numbers that aren't NaN, arrays of
// the right length. Values of the wrong type are mistakes, never silently
// turned into 0.
//
// The first problem is kept in error(), and later reads just return their
// fallback, so callers read everything and check ok() once:
//
//     js_args a(QStringLiteral("loop(start, count, body)"));
//     const int start = a.integer(start_value, "start");
//     const int count = a.integer(count_value, "count");
//     if(!a.ok()) return fail(QJSValue::TypeError, a.error());
class js_args {
    public:
        // `where` starts each message, e.g. "loop(start, count, body)".
        explicit js_args(const QString& where) : m_where(where) {}

        bool ok() const { return m_error.isEmpty(); }
        QString error() const { return m_error; }

        // Keeps `message` (after `where`) unless there's already a problem.
        void fail(const QString& message);

        // A finite number.
        double number(const QJSValue& value, const char* name, double fallback = 0.0);
        // A finite number in int's range, truncated like osu! does (times,
        // counts).
        int integer(const QJSValue& value, const char* name, int fallback = 0);
        // A string; with numbers_too also a number (osu!'s easing numbers).
        QString text(const QJSValue& value, const char* name, bool numbers_too = false);
        // [x, y] or [r, g, b]: an array of `count` (2 or 3) finite numbers,
        // stored in `out`. False, with `out` untouched, on a problem.
        bool numbers(const QJSValue& value, const char* name, int count, double* out);

        // How a value reads in a message: undefined, null, "text", 12, an array of 2, ...
        static QString describe(const QJSValue& value);

    private:
        QString m_where;
        QString m_error;
};

// Reads a script's options object ({ startTime: 0, ... }) with js_args'
// checks, plus: it must be an object, and unknown keys (typos) are
// mistakes. A missing key gives the fallback; a key that's present but
// undefined is a mistake (usually a variable that was never set).
//
//     js_options o(opts, "fade(options)", {"startTime", "endTime"});
//     const int t0 = o.integer("startTime", 0);
//     if(!o.ok()) return fail(QJSValue::TypeError, o.error());
class js_options {
    public:
        // `where` starts each message, e.g. "fade(options)". The first key
        // shows up in the example of the "must be an object" message.
        js_options(const QJSValue& opts, const QString& where, const QStringList& keys);

        bool ok() const { return m_args.ok(); }
        QString error() const { return m_args.error(); }

        bool has(const char* key) const;

        double number(const char* key, double fallback);
        int integer(const char* key, int fallback);
        QString text(const char* key, const QString& fallback, bool numbers_too = false);
        // False, with `out` untouched, when the key is missing or there's a problem.
        bool numbers(const char* key, int count, double* out);

    private:
        QJSValue value(const char* key) const;

        QJSValue m_opts;
        js_args m_args;
};
