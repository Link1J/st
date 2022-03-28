#pragma once
#include <cstdarg>
#include <cstdio>

inline void die(char const* errstr, ...)
{
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(1);
}