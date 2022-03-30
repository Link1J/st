/* See LICENSE for license details. */

#include <stdint.h>
#include <sys/types.h>

#include "config_types.hpp"
#include "support.hpp"
#include "utf8.hpp"

#ifdef XFT_VERSION
/* only exposed to x.c, otherwise we'll need Xft.h for the types */
void boxdraw_xinit(Display*, Colormap, XftDraw*, Visual*);
void drawboxes(int, int, int, int, XftColor*, XftColor*, XftGlyphFontSpec const*, int);
#endif