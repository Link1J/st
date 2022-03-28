/* See LICENSE for license details. */

#include <stdint.h>
#include <sys/types.h>

#include "support.hpp"
#include "utf8.hpp"

typedef union
{
    int         i;
    uint        ui;
    float       f;
    void const* v;
    char const* s;
} Arg;

void redraw(void);
void draw(void);

void kscrolldown(Arg const*);
void kscrollup(Arg const*);
void printscreen(Arg const*);
void printsel(Arg const*);
void sendbreak(Arg const*);
void toggleprinter(Arg const*);

void resettitle(void);

#ifdef XFT_VERSION
/* only exposed to x.c, otherwise we'll need Xft.h for the types */
void boxdraw_xinit(Display*, Colormap, XftDraw*, Visual*);
void drawboxes(int, int, int, int, XftColor*, XftColor*, XftGlyphFontSpec const*, int);
#endif