#include "scripting/js_options.h"
#include "core/name_hint.h"

#include <QJSValueIterator>

#include <algorithm>
#include <cmath>
#include <limits>

void js_args::fail(const QString& message){
    if(m_error.isEmpty()){
        m_error = QStringLiteral("%1: %2").arg(m_where, message);
    }
}

QString js_args::describe(const QJSValue& value){
    if(value.isUndefined()){
        return QStringLiteral("undefined");
    }
    if(value.isNull()){
        return QStringLiteral("null");
    }
    if(value.isString()){
        return QStringLiteral("\"%1\"").arg(value.toString());
    }
    if(value.isNumber() || value.isBool()){
        return value.toString();    // NaN and Infinity read as such
    }
    if(value.isArray()){
        return QStringLiteral("an array of %1").arg(value.property("length").toInt());
    }
    if(value.isCallable()){
        return QStringLiteral("a function");
    }

    return QStringLiteral("an object");
}

double js_args::number(const QJSValue& value, const char* name, double fallback){
    if(!ok()){
        return fallback;
    }

    if(!value.isNumber() || !std::isfinite(value.toNumber())){
        fail(QStringLiteral("%1 must be a number (got %2)").arg(QString::fromLatin1(name), describe(value)));
        return fallback;
    }

    return value.toNumber();
}

int js_args::integer(const QJSValue& value, const char* name, int fallback){
    const double read = number(value, name, fallback);

    if(!ok()){
        return fallback;
    }

    const double truncated = std::trunc(read);

    if(truncated < std::numeric_limits<int>::min() || truncated > std::numeric_limits<int>::max()){
        fail(QStringLiteral("%1 is out of range (got %2)").arg(QString::fromLatin1(name), describe(value)));
        return fallback;
    }

    return int(truncated);
}

QString js_args::text(const QJSValue& value, const char* name, bool numbers_too){
    if(!ok()){
        return {};
    }

    if(value.isString() || (numbers_too && value.isNumber())){
        return value.toString();
    }

    fail(QStringLiteral("%1 must be %2 (got %3)").arg(QString::fromLatin1(name),
            numbers_too ? QStringLiteral("a name or a number") : QStringLiteral("text"),
            describe(value)));
    return {};
}

bool js_args::numbers(const QJSValue& value, const char* name, int count, double* out){
    if(!ok()){
        return false;
    }

    const QString shape = count == 2 ? QStringLiteral("[x, y]") : QStringLiteral("[r, g, b]");

    if(!value.isArray() || value.property("length").toInt() != count){
        fail(QStringLiteral("%1 must be %2 (got %3)").arg(QString::fromLatin1(name), shape, describe(value)));
        return false;
    }

    double read[3]{};

    for(int i = 0; i < count; ++i){
        const QJSValue item = value.property(quint32(i));

        if(!item.isNumber() || !std::isfinite(item.toNumber())){
            fail(QStringLiteral("%1 must be %2 of numbers (item %3 is %4)")
                    .arg(QString::fromLatin1(name), shape).arg(i).arg(describe(item)));
            return false;
        }

        read[i] = item.toNumber();
    }

    std::copy(read, read + count, out);
    return true;
}

js_options::js_options(const QJSValue& opts, const QString& where, const QStringList& keys):
    m_opts(opts),
    m_args(where)
{
    if(!opts.isObject() || opts.isArray() || opts.isCallable()){
        m_args.fail(QStringLiteral("the options must be an object like { %1: ... } (got %2)")
                .arg(keys.value(0), js_args::describe(opts)));
        return;
    }

    QJSValueIterator it(opts);

    while(it.hasNext()){
        it.next();
        const QString key = it.name();

        if(keys.contains(key)){
            continue;
        }

        const QString hint = did_you_mean(key, keys);
        m_args.fail(hint.isEmpty()
                ? QStringLiteral("unknown option \"%1\". Use: %2").arg(key, keys.join(QStringLiteral(", ")))
                : QStringLiteral("unknown option \"%1\".%2").arg(key, hint));
        return;
    }
}

bool js_options::has(const char* key) const{
    return m_opts.isObject() && m_opts.hasOwnProperty(QString::fromLatin1(key));
}

QJSValue js_options::value(const char* key) const{
    return m_opts.property(QString::fromLatin1(key));
}

double js_options::number(const char* key, double fallback){
    return has(key) ? m_args.number(value(key), key, fallback) : fallback;
}

int js_options::integer(const char* key, int fallback){
    return has(key) ? m_args.integer(value(key), key, fallback) : fallback;
}

QString js_options::text(const char* key, const QString& fallback, bool numbers_too){
    if(!has(key) || !ok()){
        return fallback;
    }

    const QString read = m_args.text(value(key), key, numbers_too);
    return ok() ? read : fallback;
}

bool js_options::numbers(const char* key, int count, double* out){
    return has(key) && m_args.numbers(value(key), key, count, out);
}
