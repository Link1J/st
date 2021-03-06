#include "utf8.hpp"
#include "support.hpp"
#include <array>

static std::array<uchar, UTF_SIZ + 1> utfbyte = {0x80, 0, 0xC0, 0xE0, 0xF0};
static std::array<uchar, UTF_SIZ + 1> utfmask = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static std::array<Rune, UTF_SIZ + 1>  utfmin  = {0, 0, 0x80, 0x800, 0x10000};
static std::array<Rune, UTF_SIZ + 1>  utfmax  = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

size_t utf8decode(char const* c, Rune& u, size_t clen)
{
    size_t i, j, len, type;
    Rune   udecoded;

    u = UTF_INVALID;
    if (!clen)
        return 0;
    udecoded = utf8decodebyte(c[0], &len);
    if (!BETWEEN(len, 1, UTF_SIZ))
        return 1;
    for (i = 1, j = 1; i < clen && j < len; ++i, ++j)
    {
        udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
        if (type != 0)
            return j;
    }
    if (j < len)
        return 0;

    u = udecoded;
    utf8validate(u, len);

    return len;
}

Rune utf8decodebyte(char c, size_t* i)
{
    for (*i = 0; *i < LEN(utfmask); ++(*i))
        if (((uchar)c & utfmask[*i]) == utfbyte[*i])
            return (uchar)c & ~utfmask[*i];

    return 0;
}

size_t utf8encode(Rune u, char* c)
{
    size_t len, i;

    len = utf8validate(u, 0);
    if (len > UTF_SIZ)
        return 0;

    for (i = len - 1; i != 0; --i)
    {
        c[i] = utf8encodebyte(u, 0);
        u >>= 6;
    }
    c[0] = utf8encodebyte(u, len);

    return len;
}

char utf8encodebyte(Rune u, size_t i)
{
    return utfbyte[i] | (u & ~utfmask[i]);
}

size_t utf8validate(Rune& u, size_t i)
{
    if (!BETWEEN(u, utfmin[i], utfmax[i]) || BETWEEN(u, 0xD800, 0xDFFF))
        u = UTF_INVALID;
    for (i = 1; u > utfmax[i]; ++i) {}
    return i;
}