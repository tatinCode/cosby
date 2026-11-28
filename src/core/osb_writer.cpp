#include "core/osb_writer.h"
#include "core/sprite_timing.h"
#include "project/asset_paths.h"

#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <iterator>

QString osb_layer_name(layer_t layer){
    switch(layer){
        case layer_t::background: return "Background";
        case layer_t::fail:       return "Fail";
        case layer_t::pass:       return "Pass";
        case layer_t::foreground: return "Foreground";
        case layer_t::overlay:    return "Overlay";
    }

    return "Foreground";
}

QString osb_origin_name(origin_t origin){
    switch(origin){
        case origin_t::top_left:      return "TopLeft";
        case origin_t::top_center:    return "TopCentre";
        case origin_t::top_right:     return "TopRight";
        case origin_t::center_left:   return "CentreLeft";
        case origin_t::center:        return "Centre";
        case origin_t::center_right:  return "CentreRight";
        case origin_t::bottom_left:   return "BottomLeft";
        case origin_t::bottom_center: return "BottomCentre";
        case origin_t::bottom_right:  return "BottomRight";
    }

    return "Centre";
}

QString osb_number(double value){
    if(!std::isfinite(value)){
        return "0";
    }

    double rounded = std::round(value * 10000.0) / 10000.0;

    if(rounded == 0.0){
        rounded = 0.0;      // no "-0"
    }

    QString text = QString::number(rounded, 'f', 4);

    while(text.endsWith('0')){
        text.chop(1);
    }

    if(text.endsWith('.')){
        text.chop(1);
    }

    return text;
}

namespace {
    QString colour_channel(double value){
        return QString::number(std::clamp(int(std::lround(value)), 0, 255));
    }

    bool is_parameter(cmd_t type){
        return type == cmd_t::flip_h || type == cmd_t::flip_v || type == cmd_t::additive;
    }

    QString command_code(cmd_t type){
        switch(type){
            case cmd_t::fade:      return "F";
            case cmd_t::move:      return "M";
            case cmd_t::move_x:    return "MX";
            case cmd_t::move_y:    return "MY";
            case cmd_t::scale:     return "S";
            case cmd_t::scale_vec: return "V";
            case cmd_t::rotate:    return "R";
            case cmd_t::color:     return "C";
            case cmd_t::flip_h:
            case cmd_t::flip_v:
            case cmd_t::additive:  return "P";
        }

        return "F";
    }

    // The start values, plus the end values when they differ (osu! keeps a
    // value constant when the end is left out).
    QString command_values(const command& cmd){
        QStringList start;
        QStringList end;

        switch(cmd.type){
            case cmd_t::fade:
            case cmd_t::move_x:
            case cmd_t::move_y:
            case cmd_t::scale:
            case cmd_t::rotate:
                start << osb_number(cmd.v0);
                end << osb_number(cmd.v1);
                break;

            case cmd_t::move:
            case cmd_t::scale_vec:
                start << osb_number(cmd.x0) << osb_number(cmd.y0);
                end << osb_number(cmd.x1) << osb_number(cmd.y1);
                break;

            case cmd_t::color:
                start << colour_channel(cmd.r0) << colour_channel(cmd.g0) << colour_channel(cmd.b0);
                end << colour_channel(cmd.r1) << colour_channel(cmd.g1) << colour_channel(cmd.b1);
                break;

            case cmd_t::flip_h:   return "H";
            case cmd_t::flip_v:   return "V";
            case cmd_t::additive: return "A";
        }

        return (start == end ? start : start + end).join(',');
    }

    QString sprite_label(const sprite& s){
        return s.name.isEmpty() ? QFileInfo(s.path).fileName() : s.name;
    }
}

QString write_osb(const scene& sc, QList<osb_warning>* warnings){
    auto warn = [warnings](const sprite& s, const QString& message){
        if(warnings){
            warnings->append({QString("%1: %2").arg(sprite_label(s), message), s.source_line});
        }
    };

    // One command line; depth 1 for the sprite's own, 2 inside L/T.
    auto write_command = [&warn](QString& out, const sprite& s, const command& cmd, int depth){
        int end_time = cmd.t1;

        if(cmd.t1 < cmd.t0){
            // Permanent flip/additive is spelled end = start; for anything
            // else, an end before the start is a mistake.
            if(!is_parameter(cmd.type)){
                warn(s, QString("a %1 command ends (%2 ms) before it starts (%3 ms); "
                                "exported as instant").arg(command_code(cmd.type))
                            .arg(cmd.t1).arg(cmd.t0));
            }

            end_time = cmd.t0;
        }

        const int easing = is_parameter(cmd.type) ? 0 : int(cmd.easing);

        out += QString(depth, ' ');
        out += QString("%1,%2,%3,%4,%5\n").arg(
                command_code(cmd.type), QString::number(easing),
                QString::number(cmd.t0), QString::number(end_time),
                command_values(cmd));
    };

    static constexpr layer_t layer_order[] = {
        layer_t::background, layer_t::fail, layer_t::pass, layer_t::foreground, layer_t::overlay
    };

    QString out;
    out += "[Events]\n";
    out += "//Background and Video events\n";

    // osu!'s layer numbers are the positions in this order (Cosby's own
    // layer_t values differ).
    for(int number = 0; number < int(std::size(layer_order)); ++number){
        const layer_t layer = layer_order[number];
        out += QString("//Storyboard Layer %1 (%2)\n").arg(number).arg(osb_layer_name(layer));

        for(const sprite& s : sc.sprites){
            if(s.layer != layer){
                continue;
            }

            const QString path = normalize_asset_path(s.path);

            if(!is_safe_asset_path(path)){
                warn(s, QString("image path \"%1\" must be relative to the beatmap folder, "
                                "without \"..\"; osu! won't find it").arg(s.path));
            }

            if(!has_visible_commands(s)){
                warn(s, s.triggers.isEmpty()
                        ? QString("has no commands, so osu! never shows it")
                        : QString("only has trigger commands, so osu! never shows it; "
                                  "give it a command outside the trigger (e.g. a fade to 0)"));
            }

            if(s.is_animation()){
                // osu!lazer puts the frame number before *every* dot of
                // the path, so only one dot works the same everywhere.
                if(path.count('.') != 1){
                    warn(s, QString("animation path \"%1\" should contain exactly one dot (before the "
                                    "extension), or osu! looks for other frame file names").arg(path));
                }

                out += QString("Animation,%1,%2,\"%3\",%4,%5,%6,%7,%8\n").arg(
                        osb_layer_name(s.layer), osb_origin_name(s.origin), path,
                        osb_number(s.x), osb_number(s.y),
                        QString::number(s.frame_count), osb_number(s.frame_delay),
                        s.loop_type == animation_loop_t::loop_once ? QStringLiteral("LoopOnce")
                                                                   : QStringLiteral("LoopForever"));
            }
            else{
                out += QString("Sprite,%1,%2,\"%3\",%4,%5\n").arg(
                        osb_layer_name(s.layer), osb_origin_name(s.origin), path,
                        osb_number(s.x), osb_number(s.y));
            }

            for(const command& cmd : s.cmds){
                write_command(out, s, cmd, 1);
            }

            for(const loop_group& loop : s.loops){
                if(loop.cmds.isEmpty()){
                    warn(s, QString("the loop at %1 ms has no commands; left out").arg(loop.start));
                    continue;
                }

                out += QString(" L,%1,%2\n").arg(QString::number(loop.start), QString::number(loop_passes(loop)));

                for(const command& cmd : loop.cmds){
                    write_command(out, s, cmd, 2);
                }
            }

            for(const trigger_group& trigger : s.triggers){
                if(trigger.cmds.isEmpty()){
                    warn(s, QString("the %1 trigger has no commands; left out").arg(trigger.trigger));
                    continue;
                }

                out += QString(" T,%1,%2,%3").arg(trigger.trigger, QString::number(trigger.start),
                                                  QString::number(trigger.end));

                if(trigger.group != 0){
                    out += ',' + QString::number(trigger.group);
                }

                out += '\n';

                for(const command& cmd : trigger.cmds){
                    write_command(out, s, cmd, 2);
                }
            }
        }
    }

    out += "//Storyboard Sound Samples\n";

    for(const sample& smp : sc.samples){
        const QString path = normalize_asset_path(smp.path);

        if(!is_safe_asset_path(path) && warnings){
            warnings->append({QString("%1: sound path must be relative to the beatmap folder, "
                                      "without \"..\"; osu! won't find it").arg(smp.path), smp.source_line});
        }

        out += QString("Sample,%1,%2,\"%3\",%4\n").arg(
                QString::number(smp.time), QString::number(osb_sample_layer_number(smp.layer)),
                path, QString::number(std::clamp(smp.volume, 0, 100)));
    }

    return out;
}

int osb_sample_layer_number(layer_t layer){
    switch(layer){
        case layer_t::background: return 0;
        case layer_t::fail:       return 1;
        case layer_t::pass:       return 2;
        case layer_t::foreground:
        case layer_t::overlay:    return 3;
    }

    return 3;
}
