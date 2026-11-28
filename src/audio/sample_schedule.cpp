#include "audio/sample_schedule.h"

#include <algorithm>

sample_schedule::sample_schedule(const QVector<sample>& samples){
    for(qsizetype i = 0; i < samples.size(); ++i){
        if(plays_in_preview(samples[i].layer)){
            m_order.append({samples[i].time, i});
        }
    }

    std::stable_sort(m_order.begin(), m_order.end(), [](const entry& a, const entry& b){
        return a.time < b.time;
    });
}

QVector<qsizetype> sample_schedule::due(qint64 after, qint64 up_to) const{
    QVector<qsizetype> out;

    const qint64 from = std::max(after, up_to - late_limit_ms - 1);

    if(up_to <= from){
        return out;
    }

    auto it = std::upper_bound(m_order.begin(), m_order.end(), from, [](qint64 t, const entry& e){
        return t < e.time;
    });

    for(; it != m_order.end() && it->time <= up_to; ++it){
        out.append(it->index);
    }

    return out;
}

bool sample_schedule::plays_in_preview(layer_t layer){
    return layer != layer_t::fail && layer != layer_t::overlay;
}
