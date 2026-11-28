#pragma once

#include "core/scene.h"

#include <QStringList>

#include <algorithm>

// A command's end, treating end < start (instants, permanent flags) as start.
inline int command_end(const command& cmd){
    return std::max(cmd.t0, cmd.t1);
}

// The inner commands of a loop, relative to its start: when the first one
// starts, when the last one ends, and so how long one pass is.
struct loop_timing{
    bool valid = false;     // false for an empty loop
    int first = 0;
    int last = 0;

    int duration() const { return last - first; }
};

inline loop_timing timing_of(const loop_group& loop){
    loop_timing timing;

    for(const command& cmd : loop.cmds){
        if(!timing.valid){
            timing = {true, cmd.t0, command_end(cmd)};
        }
        else{
            timing.first = std::min(timing.first, cmd.t0);
            timing.last = std::max(timing.last, command_end(cmd));
        }
    }

    return timing;
}

inline int loop_passes(const loop_group& loop){
    return std::max(1, loop.count);
}

// Time span (absolute) in which something exists.
struct time_span{
    bool valid = false;     // false when there is nothing to time
    int start = 0;
    int end = 0;

    void include(int t0, int t1){
        if(!valid){
            *this = {true, t0, t1};
            return;
        }

        start = std::min(start, t0);
        end = std::max(end, t1);
    }
};

// All passes of a loop, absolute: osu! runs from the first inner command of
// the first pass to the end of the last pass.
inline time_span span_of(const loop_group& loop){
    const loop_timing timing = timing_of(loop);

    if(!timing.valid){
        return {};
    }

    const int begin = loop.start + timing.first;
    return {true, begin, begin + timing.duration() * loop_passes(loop)};
}

/**
 * When osu! shows the sprite: from its earliest command to its latest end,
 * loops included (all their passes). Triggers don't count: a sprite with only
 * trigger commands never shows.
 */
inline time_span lifetime_of(const sprite& s){
    time_span life;

    for(const command& cmd : s.cmds){
        life.include(cmd.t0, command_end(cmd));
    }

    for(const loop_group& loop : s.loops){
        const time_span span = span_of(loop);

        if(span.valid){
            life.include(span.start, span.end);
        }
    }

    return life;
}

// Whether osu! draws the sprite at all (it skips sprites without commands).
inline bool has_visible_commands(const sprite& s){
    return lifetime_of(s).valid;
}

// Frame `index` of an animation: the number goes before the extension,
// "sb/flame.png" -> "sb/flame3.png". Without an extension it is appended.
inline QString frame_path(const QString& path, int index){
    const int slash = std::max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
    const int dot = path.lastIndexOf('.');
    const QString number = QString::number(index);

    if(dot <= slash){
        return path + number;
    }

    return path.left(dot) + number + path.mid(dot);
}

// Every image file the sprite uses: its path, or each animation frame.
inline QStringList image_paths(const sprite& s){
    if(!s.is_animation()){
        return {s.path};
    }

    QStringList paths;
    paths.reserve(s.frame_count);

    for(int i = 0; i < s.frame_count; ++i){
        paths << frame_path(s.path, i);
    }

    return paths;
}

/**
 * The animation frame shown at ms. Frames are counted from the sprite's
 * first command (as in osu!); LoopOnce stops on the last frame.
 */
inline int animation_frame(const sprite& s, int ms){
    if(!s.is_animation()){
        return 0;
    }

    const time_span life = lifetime_of(s);

    if(!life.valid || ms <= life.start || s.frame_delay <= 0.0){
        return 0;
    }

    const auto frame = qint64((ms - life.start) / s.frame_delay);

    if(s.loop_type == animation_loop_t::loop_once){
        return int(std::min<qint64>(frame, s.frame_count - 1));
    }

    return int(frame % s.frame_count);
}
