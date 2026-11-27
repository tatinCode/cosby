#include "core/time_format.h"

#include <QRegularExpression>

#include <cstdlib>

namespace {
    constexpr qint64 max_time_ms = 24LL * 60 * 60 * 1000;

    // "4" -> 400, "45" -> 450, "456" -> 456: a decimal fraction of a second.
    qint64 fraction_ms(const QString& digits){
        if(digits.isEmpty()){
            return 0;
        }

        return QString(digits + QStringLiteral("00")).left(3).toLongLong();
    }

    // An unsigned duration in one of the accepted forms.
    std::optional<qint64> parse_duration(const QString& text){
        static const QRegularExpression plain_ms(QStringLiteral("^(\\d{1,9})$"));
        static const QRegularExpression seconds(QStringLiteral("^(\\d{1,6})(?:\\.(\\d{1,3}))?\\s*s$"));
        static const QRegularExpression min_sec(QStringLiteral("^(\\d{1,4}):(\\d{1,2})(?:\\.(\\d{1,3}))?$"));
        static const QRegularExpression osu(QStringLiteral("^(\\d{1,4}):(\\d{2}):(\\d{3})$"));

        QRegularExpressionMatch m;

        if((m = plain_ms.match(text)).hasMatch()){
            return m.captured(1).toLongLong();
        }

        if((m = seconds.match(text)).hasMatch()){
            return m.captured(1).toLongLong() * 1000 + fraction_ms(m.captured(2));
        }

        const bool is_osu = (m = osu.match(text)).hasMatch();
        if(is_osu || (m = min_sec.match(text)).hasMatch()){
            const qint64 minutes = m.captured(1).toLongLong();
            const qint64 secs = m.captured(2).toLongLong();

            if(secs >= 60){
                return std::nullopt;
            }

            const qint64 millis = is_osu ? m.captured(3).toLongLong() : fraction_ms(m.captured(3));
            return minutes * 60000 + secs * 1000 + millis;
        }

        return std::nullopt;
    }
}

QString format_osu_timestamp(qint64 ms){
    const QString sign = ms < 0 ? QStringLiteral("-") : QString();
    ms = std::llabs(ms);

    return QStringLiteral("%1%2:%3:%4").arg(
            sign,
            QStringLiteral("%1").arg(ms / 60000, 2, 10, QChar('0')),
            QStringLiteral("%1").arg((ms / 1000) % 60, 2, 10, QChar('0')),
            QStringLiteral("%1").arg(ms % 1000, 3, 10, QChar('0')));
}

std::optional<qint64> parse_time_input(const QString& input, qint64 current_ms){
    QString text = input.trimmed();

    // osu! copies selections as "01:23:456 (1,2,3) - "; keep the timestamp.
    static const QRegularExpression osu_suffix(QStringLiteral("^(\\d+:\\d{2}:\\d{3})\\s*(?:\\([^)]*\\))?\\s*-?$"));
    const auto suffix = osu_suffix.match(text);
    if(suffix.hasMatch()){
        text = suffix.captured(1);
    }

    int sign = 0;
    if(text.startsWith('+') || text.startsWith('-')){
        sign = text.startsWith('+') ? 1 : -1;
        text = text.mid(1).trimmed();
    }

    const auto duration = parse_duration(text);
    if(!duration || *duration > max_time_ms){
        return std::nullopt;
    }

    const qint64 result = sign == 0 ? *duration : current_ms + sign * *duration;
    if(std::llabs(result) > max_time_ms){
        return std::nullopt;
    }

    return result;
}
