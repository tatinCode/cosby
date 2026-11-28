#include "audio/playback_clock.h"

#include <algorithm>
#include <cmath>

void playback_clock::play(qint64 now){
    if(m_playing){
        return;
    }

    m_anchor_now = now;
    m_last_ms = qint64(std::llround(m_anchor_ms));
    m_playing = true;
}

void playback_clock::pause(qint64 now){
    if(!m_playing){
        return;
    }

    // Stop where the screen was, not a few ms back after a correction.
    anchor(double(time(now)), now);
    m_playing = false;
}

void playback_clock::seek(qint64 ms, qint64 now){
    anchor(double(ms), now);
    m_last_ms = ms;
}

void playback_clock::set_rate(double rate, qint64 now){
    if(rate <= 0 || rate == m_rate){
        return;
    }

    anchor(m_playing ? double(time(now)) : m_anchor_ms, now);
    m_rate = rate;
}

void playback_clock::song_position(qint64 position_ms, qint64 now){
    if(!m_playing){
        return;
    }

    const double drift = double(position_ms) - raw_time(now);

    if(std::abs(drift) > snap_threshold_ms){
        anchor(double(position_ms), now);

        if(drift < -double(hold_limit_ms)){
            m_last_ms = position_ms;
        }

        return;
    }

    m_anchor_ms += drift * correction_gain;
}

qint64 playback_clock::time(qint64 now){
    if(!m_playing){
        return qint64(std::llround(m_anchor_ms));
    }

    m_last_ms = std::max(m_last_ms, qint64(std::llround(raw_time(now))));
    return m_last_ms;
}

double playback_clock::raw_time(qint64 now) const{
    if(!m_playing){
        return m_anchor_ms;
    }

    return m_anchor_ms + double(now - m_anchor_now) * m_rate;
}

void playback_clock::anchor(double ms, qint64 now){
    m_anchor_ms = ms;
    m_anchor_now = now;
}
