#pragma once

#include "support.hpp"
#include <chrono>
#include <time.h>

inline constexpr auto TIMEDIFF(auto&& t1, auto&& t2)
{
    return (t1.tv_sec - t2.tv_sec) * 1000 + (t1.tv_nsec - t2.tv_nsec) / 1E6;
}

inline int                                   su = 0;
inline std::chrono::steady_clock::time_point sutv;

inline void tsync_begin()
{
    sutv = std::chrono::steady_clock::now();
    su   = 1;
}

inline void tsync_end()
{
    su = 0;
}

inline int tinsync(uint timeout)
{
    auto now = std::chrono::steady_clock::now();
    if (su && (now - sutv).count() >= timeout)
        su = 0;
    return su;
}