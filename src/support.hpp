#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <type_traits>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, a, b) ((a) <= (x) && (x) <= (b))
#define DIVCEIL(n, d) (((n) + ((d)-1)) / (d))
#define DEFAULT(a, b) (a) = (a) ? (a) : (b)
#define LIMIT(x, a, b) (x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define MODBIT(x, set, bit) ((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))

#define TRUECOLOR(r, g, b) (1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x) (1 << 24 & (x))

using uchar  = unsigned char;
using uint   = unsigned int;
using ulong  = unsigned long;
using ushort = unsigned short;

inline void die(char const* errstr, ...)
{
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(1);
}

template<typename T>
auto xmalloc(size_t len)
{
    constexpr auto temp = []{
        if constexpr (!(std::is_unbounded_array_v<T> || std::is_pointer_v<T>))
            throw 1;
        return 0;
    };

    if constexpr (std::is_unbounded_array_v<T>)
        len = sizeof(T) * len;

    auto p = (T*)malloc(len);
    if (p == nullptr)
        die("malloc: %s\n", strerror(errno));
    return p;
}

template<typename T>
T* xrealloc(T* p, size_t len)
{
    p = (T*)realloc(p, len);
    if (p == nullptr)
        die("realloc: %s\n", strerror(errno));
    return p;
}

inline char* xstrdup(char* s)
{
    if ((s = strdup(s)) == NULL)
        die("strdup: %s\n", strerror(errno));
    return s;
}

#if !defined(_WIN32)
#include <unistd.h>

inline ssize_t xwrite(int fd, char const* s, size_t len)
{
    size_t  aux = len;
    ssize_t r;

    while (len > 0)
    {
        r = write(fd, s, len);
        if (r < 0)
            return r;
        len -= r;
        s += r;
    }

    return aux;
}
#endif