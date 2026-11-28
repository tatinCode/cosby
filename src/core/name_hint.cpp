#include "core/name_hint.h"

#include <QVector>

#include <algorithm>
#include <utility>

namespace {
    // Edit distance, ignoring case: how many letters to add, remove, change
    // or swap with their neighbour ("Fial" -> "Fail" is 1) to get from one
    // to the other (optimal string alignment distance).
    int edit_distance(const QString& a, const QString& b){
        const QString x = a.toCaseFolded();
        const QString y = b.toCaseFolded();
        const qsizetype width = y.size() + 1;

        // Three rows: two before the current one, for swaps.
        QVector<int> before(width), previous(width), current(width);

        for(qsizetype j = 0; j < width; ++j){
            previous[j] = int(j);
        }

        for(qsizetype i = 1; i <= x.size(); ++i){
            current[0] = int(i);

            for(qsizetype j = 1; j < width; ++j){
                const int cost = x[i - 1] == y[j - 1] ? 0 : 1;
                current[j] = std::min({previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost});

                if(i > 1 && j > 1 && x[i - 1] == y[j - 2] && x[i - 2] == y[j - 1]){
                    current[j] = std::min(current[j], before[j - 2] + 1);
                }
            }

            std::swap(before, previous);
            std::swap(previous, current);
        }

        return previous[width - 1];
    }
}

QString closest_name(const QString& input, const QStringList& candidates){
    if(input.isEmpty()){
        return {};
    }

    const int allowed = std::max(1, int(input.size()) / 3);

    QString best;
    int best_distance = allowed + 1;

    for(const QString& candidate : candidates){
        const int distance = edit_distance(input, candidate);

        if(distance < best_distance){
            best = candidate;
            best_distance = distance;
        }
    }

    return best;
}

QString did_you_mean(const QString& input, const QStringList& candidates){
    const QString name = closest_name(input, candidates);
    return name.isEmpty() ? QString() : QStringLiteral(" Did you mean \"%1\"?").arg(name);
}
