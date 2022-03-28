#pragma once

#define Glyph Glyph_
#include <X11/Xft/Xft.h>
#include <hb.h>
#include <hb-ft.h>
#undef Glyph

#include "con/con.hpp"

void hbunloadfonts();
void hbtransform(XftGlyphFontSpec*, Glyph const*, size_t, int, int);
