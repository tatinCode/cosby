#pragma once

#include "core/scene.h"

#include <QVector>

/**
 * Which storyboard samples the preview plays, and when.
 *
 * The preview plays as if the player is passing: Fail-layer samples are
 * left out. Each tick asks for the samples in (after, up_to]; a seek just
 * starts the next interval at the new time, so skipped samples stay silent.
 */
class sample_schedule{
    public:
        // A tick that comes this late (the app stalled) drops older samples
        // instead of playing them all at once.
        static constexpr qint64 late_limit_ms = 150;

        sample_schedule() = default;
        explicit sample_schedule(const QVector<sample>& samples);

        // Indices into the samples given to the constructor, in time order
        // (then script order), for samples in (after, up_to] that are no
        // more than late_limit_ms before up_to.
        QVector<qsizetype> due(qint64 after, qint64 up_to) const;

        bool is_empty() const { return m_order.isEmpty(); }

        // True for the layers whose samples the preview plays.
        static bool plays_in_preview(layer_t layer);

    private:
        struct entry{
            qint64 time;
            qsizetype index;
        };

        QVector<entry> m_order;     // sorted by time, stable
};
