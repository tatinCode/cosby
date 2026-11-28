#include "ui/beat_snap.h"

#include <numeric>

int beat_division(qint64 tick, int subdivision){
    if(subdivision <= 1){
        return 1;
    }

    // tick / subdivision in lowest terms; its denominator is the division.
    const qint64 within = ((tick % subdivision) + subdivision) % subdivision;
    return int(subdivision / std::gcd(within, qint64(subdivision)));
}

QColor beat_division_color(int division){
    switch(division){
        case 1:  return QColor(255, 255, 255);
        case 2:  return QColor(237, 17, 33);
        case 3:  return QColor(170, 85, 221);
        case 4:  return QColor(34, 136, 238);
        case 6:  return QColor(204, 170, 0);
        case 8:  return QColor(255, 204, 34);
        case 12: return QColor(153, 119, 0);
        case 16: return QColor(119, 68, 170);
        default: return QColor(136, 136, 136);
    }
}
