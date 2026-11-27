#pragma once

#include <QString>

// osu!'s easing number for a Cosby easing name, or -1 for an unknown name.
// osu! numbers them 0 Linear, 1 Out (decelerating), 2 In (accelerating).
inline int osu_easing_number(const QString& name){
    if(name == "Linear"){
        return 0;
    }

    if(name == "Out"){
        return 1;
    }

    if(name == "In"){
        return 2;
    }

    return -1;
}
