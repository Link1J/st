/* See LICENSE for license details. */
#include "st.h"
#include "win.h"
#include "con/con.hpp"

#include <termios.h>
#include <unistd.h>

static void drawregion(int, int, int, int);

extern Con        con;
extern TermWindow win;

void sendbreak(Arg const* arg)
{
    if (tcsendbreak(con.pty.output, 0))
        perror("Error sending break");
}

void toggleprinter(Arg const* arg)
{
    con.term.mode ^= MODE_PRINT;
}

void printscreen(Arg const* arg)
{
    con.tdump();
}

void printsel(Arg const* arg)
{
    con.tdumpsel();
}

void kscrolldown(Arg const* a)
{
    int n = a->i;

    if (n < 0)
        n = con.term.row + n;

    if (n > con.term.scr)
        n = con.term.scr;

    if (con.term.scr > 0)
    {
        con.term.scr -= n;
        con.selscroll(0, -n);
        con.tfulldirt();
    }
}

void kscrollup(Arg const* a)
{
    int n = a->i;

    if (n < 0)
        n = con.term.row + n;

    if (con.term.scr <= HISTSIZE - n)
    {
        con.term.scr += n;
        con.selscroll(0, n);
        con.tfulldirt();
    }
}

void readcolonargs(char** p, int cursor, int params[][CAR_PER_ARG])
{
    int i = 0;
    for (; i < CAR_PER_ARG; i++)
        params[cursor][i] = -1;

    if (**p != ':')
        return;

    char* np = NULL;
    i        = 0;

    while (**p == ':' && i < CAR_PER_ARG)
    {
        while (**p == ':')
            (*p)++;
        params[cursor][i] = strtol(*p, &np, 10);
        *p                = np;
        i++;
    }
}

void resettitle(void)
{
    xsettitle(NULL, 0);
}

#undef TLINE
#define TLINE(y) ((y) < con.term.scr ? con.term.hist[((y) + con.term.histi - con.term.scr + HISTSIZE + 1) % HISTSIZE] : con.term.line[(y)-con.term.scr])

void drawregion(int x1, int y1, int x2, int y2)
{
    int y;

    for (y = y1; y < y2; y++)
    {
        if (!con.term.dirty[y])
            continue;

        con.term.dirty[y] = 0;
        xdrawline(TLINE(y), x1, y, x2);
    }
}

void draw(void)
{
    int cx = con.term.c.x, ocx = con.term.ocx, ocy = con.term.ocy;

    if (!xstartdraw())
        return;

    /* adjust cursor position */
    LIMIT(con.term.ocx, 0, con.term.col - 1);
    LIMIT(con.term.ocy, 0, con.term.row - 1);
    if (con.term.line[con.term.ocy][con.term.ocx].mode & ATTR_WDUMMY)
        con.term.ocx--;
    if (con.term.line[con.term.c.y][cx].mode & ATTR_WDUMMY)
        cx--;

    drawregion(0, 0, con.term.col, con.term.row);
    if (con.term.scr == 0)
        xdrawcursor(
            cx, con.term.c.y, con.term.line[con.term.c.y][cx], con.term.ocx, con.term.ocy, con.term.line[con.term.ocy][con.term.ocx], con.term.line[con.term.ocy], con.term.col
        );
    con.term.ocx = cx;
    con.term.ocy = con.term.c.y;
    xfinishdraw();
    if (ocx != con.term.ocx || ocy != con.term.ocy)
        xximspot(con.term.ocx, con.term.ocy);
}

void redraw(void)
{
    con.tfulldirt();
    draw();
}