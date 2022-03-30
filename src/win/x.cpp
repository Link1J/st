#include "config.h"

#include "arg.h"
#include "st.h"
#include "win.h"

#include "con/con.hpp"
#include "time.hpp"

Con        con;
TermWindow win;

void clipcopy(Arg const&);
void clippaste(Arg const&);
void numlock(Arg const&);
void selpaste(Arg const&);
void zoom(Arg const&);
void zoomabs(Arg const&);
void zoomreset(Arg const&);
void ttysend(Arg const&);

void xbell(void)
{}

void xdrawcursor(int, int, Glyph, int, int, Glyph, Line, int)
{}

void xdrawline(Line, int, int, int)
{}

void xfinishdraw(void)
{}

void xloadcols(void)
{}

int xsetcolorname(int, char const*)
{}

void xfreetitlestack(void)
{}

void xsettitle(char*, int)
{}

void xpushtitle(void)
{}

int xsetcursor(int)
{}

void xsetmode(int, unsigned int)
{}

void xsetpointermotion(int)
{}

void xsetsel(char*)
{}

int xstartdraw(void)
{}

void xximspot(int, int)
{}

void xdrawsixel()
{}
