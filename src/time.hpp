#pragma once

#include "support.hpp"
#include <time.h>

inline constexpr auto TIMEDIFF(auto&& t1, auto&& t2)
{
    return (t1.tv_sec - t2.tv_sec) * 1000 + (t1.tv_nsec - t2.tv_nsec) / 1E6;
}

inline int             su = 0;
inline struct timespec sutv;

inline void tsync_begin()
{
    clock_gettime(CLOCK_MONOTONIC, &sutv);
    su = 1;
}

inline void tsync_end()
{
    su = 0;
}

inline int tinsync(uint timeout)
{
    struct timespec now;
    if (su && !clock_gettime(CLOCK_MONOTONIC, &now) && TIMEDIFF(now, sutv) >= timeout)
        su = 0;
    return su;
}