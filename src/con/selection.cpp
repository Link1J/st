#include "con.hpp"

void Con::selinit(void)
{
    sel.mode = SEL_IDLE;
    sel.snap = 0;
    sel.ob.x = -1;
}

void Con::selstart(int col, int row, int snap)
{
    selclear();
    sel.mode = SEL_EMPTY;
    sel.type = SEL_REGULAR;
    sel.alt  = IS_SET(MODE_ALTSCREEN);
    sel.snap = snap;
    sel.oe.x = sel.ob.x = col;
    sel.oe.y = sel.ob.y = row;
    selnormalize();

    if (sel.snap != 0)
        sel.mode = SEL_READY;
    tsetdirt(sel.nb.y, sel.ne.y);
}

void Con::selextend(int col, int row, int type, int done)
{
    int oldey, oldex, oldsby, oldsey, oldtype;

    if (sel.mode == SEL_IDLE)
        return;
    if (done && sel.mode == SEL_EMPTY)
    {
        selclear();
        return;
    }

    oldey   = sel.oe.y;
    oldex   = sel.oe.x;
    oldsby  = sel.nb.y;
    oldsey  = sel.ne.y;
    oldtype = sel.type;

    sel.oe.x = col;
    sel.oe.y = row;
    selnormalize();
    sel.type = type;

    if (oldey != sel.oe.y || oldex != sel.oe.x || oldtype != sel.type || sel.mode == SEL_EMPTY)
        tsetdirt(MIN(sel.nb.y, oldsby), MAX(sel.ne.y, oldsey));

    sel.mode = done ? SEL_IDLE : SEL_READY;
}

void Con::selnormalize(void)
{
    int i;

    if (sel.type == SEL_REGULAR && sel.ob.y != sel.oe.y)
    {
        sel.nb.x = sel.ob.y < sel.oe.y ? sel.ob.x : sel.oe.x;
        sel.ne.x = sel.ob.y < sel.oe.y ? sel.oe.x : sel.ob.x;
    }
    else
    {
        sel.nb.x = MIN(sel.ob.x, sel.oe.x);
        sel.ne.x = MAX(sel.ob.x, sel.oe.x);
    }
    sel.nb.y = MIN(sel.ob.y, sel.oe.y);
    sel.ne.y = MAX(sel.ob.y, sel.oe.y);

    selsnap(&sel.nb.x, &sel.nb.y, -1);
    selsnap(&sel.ne.x, &sel.ne.y, +1);

    /* expand selection over line breaks */
    if (sel.type == SEL_RECTANGULAR)
        return;
    i = tlinelen(sel.nb.y);
    if (i < sel.nb.x)
        sel.nb.x = i;
    if (tlinelen(sel.ne.y) <= sel.ne.x)
        sel.ne.x = term.col - 1;
}

int Con::selected(int x, int y)
{
    if (sel.mode == SEL_EMPTY || sel.ob.x == -1 || sel.alt != IS_SET(MODE_ALTSCREEN))
        return 0;

    if (sel.type == SEL_RECTANGULAR)
        return BETWEEN(y, sel.nb.y, sel.ne.y) && BETWEEN(x, sel.nb.x, sel.ne.x);

    return BETWEEN(y, sel.nb.y, sel.ne.y) && (y != sel.nb.y || x >= sel.nb.x) && (y != sel.ne.y || x <= sel.ne.x);
}

void Con::selsnap(int* x, int* y, int direction)
{
    int    newx, newy, xt, yt;
    int    delim, prevdelim;
    Glyph *gp, *prevgp;

    switch (sel.snap)
    {
    case SNAP_WORD:
        /*
         * Snap around if the word wraps around at the end or
         * beginning of a line.
         */
        prevgp    = &TLINE(*y)[*x];
        prevdelim = ISDELIM(prevgp->u);
        for (;;)
        {
            newx = *x + direction;
            newy = *y;
            if (!BETWEEN(newx, 0, term.col - 1))
            {
                newy += direction;
                newx = (newx + term.col) % term.col;
                if (!BETWEEN(newy, 0, term.row - 1))
                    break;

                if (direction > 0)
                    yt = *y, xt = *x;
                else
                    yt = newy, xt = newx;
                if (!(TLINE(yt)[xt].mode & ATTR_WRAP))
                    break;
            }

            if (newx >= tlinelen(newy))
                break;

            gp    = &TLINE(newy)[newx];
            delim = ISDELIM(gp->u);
            if (!(gp->mode & ATTR_WDUMMY) && (delim != prevdelim || (delim && gp->u != prevgp->u)))
                break;

            *x        = newx;
            *y        = newy;
            prevgp    = gp;
            prevdelim = delim;
        }
        break;
    case SNAP_LINE:
        /*
         * Snap around if the the previous line or the current one
         * has set ATTR_WRAP at its end. Then the whole next or
         * previous line will be selected.
         */
        *x = (direction < 0) ? 0 : term.col - 1;
        if (direction < 0)
        {
            for (; *y > 0; *y += direction)
            {
                if (!(TLINE(*y - 1)[term.col - 1].mode & ATTR_WRAP))
                {
                    break;
                }
            }
        }
        else if (direction > 0)
        {
            for (; *y < term.row - 1; *y += direction)
            {
                if (!(TLINE(*y)[term.col - 1].mode & ATTR_WRAP))
                {
                    break;
                }
            }
        }
        break;
    }
}

char* Con::getsel(void)
{
    char * str, *ptr;
    int    y, bufsize, lastx, linelen;
    Glyph *gp, *last;

    if (sel.ob.x == -1)
        return NULL;

    bufsize = (term.col + 1) * (sel.ne.y - sel.nb.y + 1) * UTF_SIZ;
    ptr = str = xmalloc<char>(bufsize);

    /* append every set & selected glyph to the selection */
    for (y = sel.nb.y; y <= sel.ne.y; y++)
    {
        if ((linelen = tlinelen(y)) == 0)
        {
            *ptr++ = '\n';
            continue;
        }

        if (sel.type == SEL_RECTANGULAR)
        {
            gp    = &TLINE(y)[sel.nb.x];
            lastx = sel.ne.x;
        }
        else
        {
            gp    = &TLINE(y)[sel.nb.y == y ? sel.nb.x : 0];
            lastx = (sel.ne.y == y) ? sel.ne.x : term.col - 1;
        }
        last = &TLINE(y)[MIN(lastx, linelen - 1)];
        while (last >= gp && last->u == ' ')
            --last;

        for (; gp <= last; ++gp)
        {
            if (gp->mode & ATTR_WDUMMY)
                continue;

            ptr += utf8encode(gp->u, ptr);
        }

        /*
         * Copy and pasting of line endings is inconsistent
         * in the inconsistent terminal and GUI world.
         * The best solution seems like to produce '\n' when
         * something is copied from st and convert '\n' to
         * '\r', when something to be pasted is received by
         * st.
         * FIXME: Fix the computer world.
         */
        if ((y < sel.ne.y || lastx >= linelen) && (!(last->mode & ATTR_WRAP) || sel.type == SEL_RECTANGULAR))
            *ptr++ = '\n';
    }
    *ptr = 0;
    return str;
}

void Con::selclear(void)
{
    if (sel.ob.x == -1)
        return;
    sel.mode = SEL_IDLE;
    sel.ob.x = -1;
    tsetdirt(sel.nb.y, sel.ne.y);
}

void Con::selscroll(int orig, int n)
{
    if (sel.ob.x == -1)
        return;

    if (BETWEEN(sel.nb.y, orig, term.bot) != BETWEEN(sel.ne.y, orig, term.bot))
    {
        selclear();
    }
    else if (BETWEEN(sel.nb.y, orig, term.bot))
    {
        sel.ob.y += n;
        sel.oe.y += n;
        if (sel.ob.y < term.top || sel.ob.y > term.bot || sel.oe.y < term.top || sel.oe.y > term.bot)
        {
            selclear();
        }
        else
        {
            selnormalize();
        }
    }
}