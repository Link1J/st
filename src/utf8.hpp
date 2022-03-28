#pragma once
#include <cstdint>
#include <cstddef>

using Rune = uint_least32_t;

constexpr auto UTF_INVALID = 0xFFFD;
constexpr auto UTF_SIZ     = sizeof(Rune);

size_t utf8decode(char const* c, Rune& u, size_t clen);
Rune   utf8decodebyte(char c, size_t* i);
size_t utf8encode(Rune u, char* c);
char   utf8encodebyte(Rune u, size_t i);
size_t utf8validate(Rune& u, size_t i);