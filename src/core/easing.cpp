#include "core/easing.h"

#include <array>
#include <cmath>

namespace {
    const std::array<const char*, easing_count> names{
        "Linear", "Out", "In",
        "QuadIn", "QuadOut", "QuadInOut",
        "CubicIn", "CubicOut", "CubicInOut",
        "QuartIn", "QuartOut", "QuartInOut",
        "QuintIn", "QuintOut", "QuintInOut",
        "SineIn", "SineOut", "SineInOut",
        "ExpoIn", "ExpoOut", "ExpoInOut",
        "CircIn", "CircOut", "CircInOut",
        "ElasticIn", "ElasticOut", "ElasticHalfOut", "ElasticQuarterOut", "ElasticInOut",
        "BackIn", "BackOut", "BackInOut",
        "BounceIn", "BounceOut", "BounceInOut",
    };

    // Constants from osu!framework's DefaultEasingFunction, which osu! uses
    // for storyboards.
    constexpr double pi = 3.14159265358979323846;
    constexpr double elastic_const = 2 * pi / 0.3;
    constexpr double elastic_const2 = 0.3 / 4;
    constexpr double back_const = 1.70158;
    constexpr double back_const2 = back_const * 1.525;
    constexpr double bounce_const = 1 / 2.75;

    // Make expo and elastic curves start and end exactly at 0 and 1.
    const double expo_offset = std::pow(2, -10);
    const double elastic_offset_full = std::pow(2, -11);
    const double elastic_offset_half = std::pow(2, -10) * std::sin((0.5 - elastic_const2) * elastic_const);
    const double elastic_offset_quarter = std::pow(2, -10) * std::sin((0.25 - elastic_const2) * elastic_const);
    const double in_out_elastic_offset = std::pow(2, -10) * std::sin((1 - elastic_const2 * 1.5) * elastic_const / 1.5);

    double bounce_out(double t){
        if(t < bounce_const){
            return 7.5625 * t * t;
        }
        if(t < 2 * bounce_const){
            t -= 1.5 * bounce_const;
            return 7.5625 * t * t + 0.75;
        }
        if(t < 2.5 * bounce_const){
            t -= 2.25 * bounce_const;
            return 7.5625 * t * t + 0.9375;
        }

        t -= 2.625 * bounce_const;
        return 7.5625 * t * t + 0.984375;
    }

    double eased(easing_t easing, double t){
        // u = t - 1, for the "--time" forms of the original.
        const double u = t - 1;

        switch(easing){
            case easing_t::linear:
                return t;

            case easing_t::in:
            case easing_t::quad_in:
                return t * t;

            case easing_t::out:
            case easing_t::quad_out:
                return t * (2 - t);

            case easing_t::quad_in_out:
                return t < 0.5 ? t * t * 2 : u * u * -2 + 1;

            case easing_t::cubic_in:
                return t * t * t;

            case easing_t::cubic_out:
                return u * u * u + 1;

            case easing_t::cubic_in_out:
                return t < 0.5 ? t * t * t * 4 : u * u * u * 4 + 1;

            case easing_t::quart_in:
                return t * t * t * t;

            case easing_t::quart_out:
                return 1 - u * u * u * u;

            case easing_t::quart_in_out:
                return t < 0.5 ? t * t * t * t * 8 : u * u * u * u * -8 + 1;

            case easing_t::quint_in:
                return t * t * t * t * t;

            case easing_t::quint_out:
                return u * u * u * u * u + 1;

            case easing_t::quint_in_out:
                return t < 0.5 ? t * t * t * t * t * 16 : u * u * u * u * u * 16 + 1;

            case easing_t::sine_in:
                return 1 - std::cos(t * pi * 0.5);

            case easing_t::sine_out:
                return std::sin(t * pi * 0.5);

            case easing_t::sine_in_out:
                return 0.5 - 0.5 * std::cos(pi * t);

            case easing_t::expo_in:
                return std::pow(2, 10 * (t - 1)) + expo_offset * (t - 1);

            case easing_t::expo_out:
                return -std::pow(2, -10 * t) + 1 + expo_offset * t;

            case easing_t::expo_in_out:
                if(t < 0.5){
                    return 0.5 * (std::pow(2, 20 * t - 10) + expo_offset * (2 * t - 1));
                }
                return 1 - 0.5 * (std::pow(2, -20 * t + 10) + expo_offset * (-2 * t + 1));

            case easing_t::circ_in:
                return 1 - std::sqrt(1 - t * t);

            case easing_t::circ_out:
                return std::sqrt(1 - u * u);

            case easing_t::circ_in_out:{
                const double d = t * 2;
                if(d < 1){
                    return 0.5 - 0.5 * std::sqrt(1 - d * d);
                }
                const double e = d - 2;
                return 0.5 * std::sqrt(1 - e * e) + 0.5;
            }

            case easing_t::elastic_in:
                return -std::pow(2, -10 + 10 * t) * std::sin((1 - elastic_const2 - t) * elastic_const)
                    + elastic_offset_full * (1 - t);

            case easing_t::elastic_out:
                return std::pow(2, -10 * t) * std::sin((t - elastic_const2) * elastic_const)
                    + 1 - elastic_offset_full * t;

            case easing_t::elastic_half_out:
                return std::pow(2, -10 * t) * std::sin((0.5 * t - elastic_const2) * elastic_const)
                    + 1 - elastic_offset_half * t;

            case easing_t::elastic_quarter_out:
                return std::pow(2, -10 * t) * std::sin((0.25 * t - elastic_const2) * elastic_const)
                    + 1 - elastic_offset_quarter * t;

            case easing_t::elastic_in_out:{
                const double d = t * 2;
                if(d < 1){
                    return -0.5 * (std::pow(2, -10 + 10 * d) * std::sin((1 - elastic_const2 * 1.5 - d) * elastic_const / 1.5)
                                   - in_out_elastic_offset * (1 - d));
                }
                const double e = d - 1;
                return 0.5 * (std::pow(2, -10 * e) * std::sin((e - elastic_const2 * 1.5) * elastic_const / 1.5)
                              - in_out_elastic_offset * e) + 1;
            }

            case easing_t::back_in:
                return t * t * ((back_const + 1) * t - back_const);

            case easing_t::back_out:
                return u * u * ((back_const + 1) * u + back_const) + 1;

            case easing_t::back_in_out:{
                const double d = t * 2;
                if(d < 1){
                    return 0.5 * d * d * ((back_const2 + 1) * d - back_const2);
                }
                const double e = d - 2;
                return 0.5 * (e * e * ((back_const2 + 1) * e + back_const2) + 2);
            }

            case easing_t::bounce_in:
                return 1 - bounce_out(1 - t);

            case easing_t::bounce_out:
                return bounce_out(t);

            case easing_t::bounce_in_out:
                if(t < 0.5){
                    return 0.5 - 0.5 * bounce_out(1 - t * 2);
                }
                return bounce_out((t - 0.5) * 2) * 0.5 + 0.5;
        }

        return t;
    }
}

QString easing_name(easing_t easing){
    const int index = int(easing);
    return (index >= 0 && index < easing_count) ? QString::fromLatin1(names[index]) : QStringLiteral("Linear");
}

QStringList easing_names(){
    QStringList list;
    list.reserve(easing_count);

    for(const char* name : names){
        list << QString::fromLatin1(name);
    }

    return list;
}

bool parse_easing(const QString& text, easing_t& out){
    const QString trimmed = text.trimmed();

    bool is_number = false;
    const int number = trimmed.toInt(&is_number);

    if(is_number){
        if(number < 0 || number >= easing_count){
            return false;
        }

        out = easing_t(number);
        return true;
    }

    for(int i = 0; i < easing_count; ++i){
        if(trimmed.compare(QLatin1String(names[i]), Qt::CaseInsensitive) == 0){
            out = easing_t(i);
            return true;
        }
    }

    return false;
}

double apply_easing(easing_t easing, double t){
    if(t <= 0.0){
        return 0.0;
    }

    if(t >= 1.0){
        return 1.0;
    }

    return eased(easing, t);
}
