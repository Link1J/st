#include "con.hpp"
#include "../st.h"
#include "../win.h"
#include "../time.hpp"
#include "../base64.hpp"
#include <limits.h>

extern TermWindow win;

void Con::csiparse()
{
    char *   p = csiescseq.buf.data(), *np;
    long int v;

    csiescseq.narg = 0;
    if (*p == '?')
    {
        csiescseq.priv = 1;
        p++;
    }

    csiescseq.buf[csiescseq.len] = '\0';
    while (p < csiescseq.buf.data() + csiescseq.len)
    {
        np = NULL;
        v  = strtol(p, &np, 10);
        if (np == p)
            v = 0;
        if (v == LONG_MAX || v == LONG_MIN)
            v = -1;
        csiescseq.arg[csiescseq.narg++] = v;
        p                               = np;
        readcolonargs(&p, csiescseq.narg - 1, csiescseq.carg);
        if (*p != ';' || csiescseq.narg == ESC_ARG_SIZ)
            break;
        p++;
    }
    csiescseq.mode[0] = *p++;
    csiescseq.mode[1] = (p < csiescseq.buf.data() + csiescseq.len) ? *p : '\0';
}

void Con::csihandle()
{
    switch (csiescseq.mode[0])
    {
    default:
    unknown:
        fprintf(stderr, "erresc: unknown csi ");
        csidump();
        /* die(""); */
        break;

    case '@': /* ICH -- Insert <n> blank char */
        DEFAULT(csiescseq.arg[0], 1);
        tinsertblank(csiescseq.arg[0]);
        break;

    case 'A': /* CUU -- Cursor <n> Up */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(term.c.x, term.c.y - csiescseq.arg[0]);
        break;

    case 'B': /* CUD -- Cursor <n> Down */
    case 'e': /* VPR --Cursor <n> Down */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(term.c.x, term.c.y + csiescseq.arg[0]);
        break;

    case 'i': /* MC -- Media Copy */
        switch (csiescseq.arg[0])
        {
        case 0:
            tdump();
            break;
        case 1:
            tdumpline(term.c.y);
            break;
        case 2:
            tdumpsel();
            break;
        case 4:
            term.mode &= ~MODE_PRINT;
            break;
        case 5:
            term.mode |= MODE_PRINT;
            break;
        }
        break;

    case 'c': /* DA -- Device Attributes */
        if (csiescseq.arg[0] == 0)
            ttywrite(vtiden, 0);
        break;

    case 'b': /* REP -- if last char is printable print it <n> more times */
        DEFAULT(csiescseq.arg[0], 1);
        if (term.lastc)
            while (csiescseq.arg[0]-- > 0)
                tputc(term.lastc);
        break;

    case 'C': /* CUF -- Cursor <n> Forward */
    case 'a': /* HPR -- Cursor <n> Forward */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(term.c.x + csiescseq.arg[0], term.c.y);
        break;

    case 'D': /* CUB -- Cursor <n> Backward */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(term.c.x - csiescseq.arg[0], term.c.y);
        break;

    case 'E': /* CNL -- Cursor <n> Down and first col */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(0, term.c.y + csiescseq.arg[0]);
        break;

    case 'F': /* CPL -- Cursor <n> Up and first col */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(0, term.c.y - csiescseq.arg[0]);
        break;

    case 'g': /* TBC -- Tabulation clear */
        switch (csiescseq.arg[0])
        {
        case 0: /* clear current tab stop */
            term.tabs[term.c.x] = 0;
            break;
        case 3: /* clear all the tabs */
            memset(term.tabs.data(), 0, term.col * sizeof(term.tabs[0]));
            break;
        default:
            goto unknown;
        }
        break;

    case 'G': /* CHA -- Move to <col> */
    case '`': /* HPA */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveto(csiescseq.arg[0] - 1, term.c.y);
        break;
    case 'H': /* CUP -- Move to <row> <col> */
    case 'f': /* HVP */
        DEFAULT(csiescseq.arg[0], 1);
        DEFAULT(csiescseq.arg[1], 1);
        tmoveato(csiescseq.arg[1] - 1, csiescseq.arg[0] - 1);
        break;

    case 'I': /* CHT -- Cursor Forward Tabulation <n> tab stops */
        DEFAULT(csiescseq.arg[0], 1);
        tputtab(csiescseq.arg[0]);
        break;

    case 'J': /* ED -- Clear screen */
        switch (csiescseq.arg[0])
        {
        case 0: /* below */
            tclearregion(term.c.x, term.c.y, term.col - 1, term.c.y);
            if (term.c.y < term.row - 1)
                tclearregion(0, term.c.y + 1, term.col - 1, term.row - 1);
            break;
        case 1: /* above */
            if (term.c.y > 1)
                tclearregion(0, 0, term.col - 1, term.c.y - 1);
            tclearregion(0, term.c.y, term.c.x, term.c.y);
            break;
        case 2: /* all */
            tclearregion(0, 0, term.col - 1, term.row - 1);
            break;
        default:
            goto unknown;
        }
        break;

    case 'K': /* EL -- Clear line */
        switch (csiescseq.arg[0])
        {
        case 0: /* right */
            tclearregion(term.c.x, term.c.y, term.col - 1, term.c.y);
            break;
        case 1: /* left */
            tclearregion(0, term.c.y, term.c.x, term.c.y);
            break;
        case 2: /* all */
            tclearregion(0, term.c.y, term.col - 1, term.c.y);
            break;
        }
        break;

    case 'S': /* SU -- Scroll <n> line up */
        DEFAULT(csiescseq.arg[0], 1);
        tscrollup(term.top, csiescseq.arg[0], 0);
        break;

    case 'T': /* SD -- Scroll <n> line down */
        DEFAULT(csiescseq.arg[0], 1);
        tscrolldown(term.top, csiescseq.arg[0], 0);
        break;

    case 'L': /* IL -- Insert <n> blank lines */
        DEFAULT(csiescseq.arg[0], 1);
        tinsertblankline(csiescseq.arg[0]);
        break;

    case 'l': /* RM -- Reset Mode */
        tsetmode(csiescseq.priv, 0, csiescseq.arg.data(), csiescseq.narg);
        break;

    case 'M': /* DL -- Delete <n> lines */
        DEFAULT(csiescseq.arg[0], 1);
        tdeleteline(csiescseq.arg[0]);
        break;

    case 'X': /* ECH -- Erase <n> char */
        DEFAULT(csiescseq.arg[0], 1);
        tclearregion(term.c.x, term.c.y, term.c.x + csiescseq.arg[0] - 1, term.c.y);
        break;

    case 'P': /* DCH -- Delete <n> char */
        DEFAULT(csiescseq.arg[0], 1);
        tdeletechar(csiescseq.arg[0]);
        break;

    case 'Z': /* CBT -- Cursor Backward Tabulation <n> tab stops */
        DEFAULT(csiescseq.arg[0], 1);
        tputtab(-csiescseq.arg[0]);
        break;

    case 'd': /* VPA -- Move to <row> */
        DEFAULT(csiescseq.arg[0], 1);
        tmoveato(term.c.x, csiescseq.arg[0] - 1);
        break;

    case 'h': /* SM -- Set terminal mode */
        tsetmode(csiescseq.priv, 1, csiescseq.arg.data(), csiescseq.narg);
        break;

    case 'm': /* SGR -- Terminal attribute (color) */
        tsetattr(csiescseq.arg.data(), csiescseq.narg);
        break;

    case 'n': /* DSR – Device Status Report (cursor position) */
        if (csiescseq.arg[0] != 6)
        {
            char buf[40];
            auto len = snprintf(buf, sizeof(buf), "\033[%i;%iR", term.c.y + 1, term.c.x + 1);
            ttywrite({buf, (size_t)len}, 0);
        }
        break;

    case 'r': /* DECSTBM -- Set Scrolling Region */
        if (csiescseq.priv)
            goto unknown;
        else
        {
            DEFAULT(csiescseq.arg[0], 1);
            DEFAULT(csiescseq.arg[1], term.row);
            tsetscroll(csiescseq.arg[0] - 1, csiescseq.arg[1] - 1);
            tmoveato(0, 0);
        }
        break;

    case 's': /* DECSC -- Save cursor position (ANSI.SYS) */
        tcursor(CURSOR_SAVE);
        break;

    case 'u': /* DECRC -- Restore cursor position (ANSI.SYS) */
        tcursor(CURSOR_LOAD);
        break;

    case ' ':
        switch (csiescseq.mode[1])
        {
        case 'q': /* DECSCUSR -- Set Cursor Style */
            if (xsetcursor(csiescseq.arg[0]))
                goto unknown;
            break;
        default:
            goto unknown;
        }
        break;

    case 't': /* title stack operations */
        switch (csiescseq.arg[0])
        {
        case 22: /* pust current title on stack */
            switch (csiescseq.arg[1])
            {
            case 0:
            case 1:
            case 2:
                xpushtitle();
                break;
            default:
                goto unknown;
            }
            break;

        case 23: /* pop last title from stack */
            switch (csiescseq.arg[1])
            {
            case 0:
            case 1:
            case 2:
                xsettitle(NULL, 1);
                break;
            default:
                goto unknown;
            }
            break;

        case 14: /* get window size */
        {
            char buf[40];
            auto len = snprintf(buf, sizeof(buf), "\033[4;%d;%dt", win.th, win.tw);
            ttywrite({buf, (size_t)len}, 0);
            break;
        }

        case 18: /* get term size */
        {
            char buf[40];
            auto len = snprintf(buf, sizeof(buf), "\033[8;%d;%dt", term.row, term.col);
            ttywrite({buf, (size_t)len}, 0);
            break;
        }

        default:
            goto unknown;
        }
    }
}

void Con::csidump()
{
    size_t i;
    uint   c;

    fprintf(stderr, "(%c) ESC[", csiescseq.mode[0]);
    for (i = 0; i < csiescseq.len; i++)
    {
        c = csiescseq.buf[i] & 0xff;
        if (isprint(c))
            putc(c, stderr);
        else if (c == '\n')
            fprintf(stderr, "(\\n)");
        else if (c == '\r')
            fprintf(stderr, "(\\r)");
        else if (c == 0x1b)
            fprintf(stderr, "(\\e)");
        else
            fprintf(stderr, "(%02x)", c);
    }
    putc('\n', stderr);
}

void Con::csireset()
{
    memset(&csiescseq, 0, sizeof(csiescseq));
}

void Con::strhandle()
{
    char *p = NULL, *dec;
    int   j, narg, par;

    term.esc &= ~(ESC_STR_END | ESC_STR);
    strescseq.buf[strescseq.len] = '\0';

    switch (strescseq.type)
    {
    case ']': /* OSC -- Operating System Command */
        strparse();
        par = (narg = strescseq.narg) ? atoi(STRESCARGJUST(0)) : 0;
        switch (par)
        {
        case 0:
        case 1:
        case 2:
            if (narg > 1)
                xsettitle(STRESCARGREST(1), 0);
            return;
        case 52:
            if (narg > 2 && allowwindowops)
            {
                dec = base64dec(STRESCARGJUST(2));
                if (dec)
                {
                    xsetsel(dec);
                    xclipcopy();
                }
                else
                {
                    fprintf(stderr, "erresc: invalid base64\n");
                }
            }
            return;
        case 4: /* color set */
            if (narg < 3)
                break;
            p = STRESCARGJUST(2);
            [[fallthrough]];
        case 104: /* color reset, here p = NULL */
            j = (narg > 1) ? atoi(STRESCARGJUST(1)) : -1;
            if (xsetcolorname(j, p))
            {
                if (par == 104 && narg <= 1)
                    return; /* color reset without parameter */
                fprintf(stderr, "erresc: invalid color j=%d, p=%s\n", j, p ? p : "(null)");
            }
            else
            {
                /*
                 * TODO if defaultbg color is changed, borders
                 * are dirty
                 */
                redraw();
            }
            return;

        case 11:
            ttywrite("11;rgb:ffff/ffff/ffff", 1);
            return;

        case 10:
            ttywrite("10;rgb:0000/0000/0000", 1);
            return;
        }
        break;

    case 'k': /* old title set compatibility */
        xsettitle(strescseq.buf.data(), 0);
        return;

    case 'P': /* DCS -- Device Control String */
        /* https://gitlab.com/gnachman/iterm2/-/wikis/synchronized-updates-spec */
        if (strescseq.buf.find("=1s") == 0)
            tsync_begin(); /* BSU */
        else if (strescseq.buf.find("=2s") == 0)
            tsync_end(); /* ESU */
        return;

    case '_': /* APC -- Application Program Command */
    case '^': /* PM -- Privacy Message */
        return;
    }

    fprintf(stderr, "erresc: unknown str ");
    strdump();
}

void Con::strparse()
{
    int   c;
    char* p = strescseq.buf.data();

    strescseq.narg = 0;

    if (*p == '\0')
        return;

    while (strescseq.narg < STR_ARG_SIZ)
    {
        while ((c = *p) != ';' && c != '\0')
            p++;
        strescseq.argp[strescseq.narg++] = p;
        if (c == '\0')
            return;
        p++;
    }
}

void Con::strdump()
{
    size_t i;
    uint   c;

    fprintf(stderr, "ESC%c", strescseq.type);
    for (i = 0; i < strescseq.len; i++)
    {
        c = strescseq.buf[i] & 0xff;
        if (c == '\0')
        {
            putc('\n', stderr);
            return;
        }
        else if (isprint(c))
        {
            putc(c, stderr);
        }
        else if (c == '\n')
        {
            fprintf(stderr, "(\\n)");
        }
        else if (c == '\r')
        {
            fprintf(stderr, "(\\r)");
        }
        else if (c == 0x1b)
        {
            fprintf(stderr, "(\\e)");
        }
        else
        {
            fprintf(stderr, "(%02x)", c);
        }
    }
    fprintf(stderr, "ESC\\\n");
}

void Con::strreset()
{
    strescseq.buf.clear();
}

/*
 * returns 1 when the sequence is finished and it hasn't to read
 * more characters for this sequence, otherwise 0
 */
int Con::eschandle(uchar ascii)
{
    switch (ascii)
    {
    case '[':
        term.esc |= ESC_CSI;
        return 0;

    case '#':
        term.esc |= ESC_TEST;
        return 0;

    case '%':
        term.esc |= ESC_UTF8;
        return 0;

    case 'P': /* DCS -- Device Control String */
        term.esc |= ESC_DCS;
        [[fallthrough]];
    case '_': /* APC -- Application Program Command */
    case '^': /* PM -- Privacy Message */
    case ']': /* OSC -- Operating System Command */
    case 'k': /* old title set compatibility */
        tstrsequence(ascii);
        return 0;

    case 'n': /* LS2 -- Locking shift 2 */
    case 'o': /* LS3 -- Locking shift 3 */
        term.charset = 2 + (ascii - 'n');
        break;

    case '(': /* GZD4 -- set primary charset G0 */
    case ')': /* G1D4 -- set secondary charset G1 */
    case '*': /* G2D4 -- set tertiary charset G2 */
    case '+': /* G3D4 -- set quaternary charset G3 */
        term.icharset = ascii - '(';
        term.esc |= ESC_ALTCHARSET;
        return 0;

    case 'D': /* IND -- Linefeed */
        if (term.c.y == term.bot)
            tscrollup(term.top, 1, 1);
        else
            tmoveto(term.c.x, term.c.y + 1);
        break;

    case 'E':        /* NEL -- Next line */
        tnewline(1); /* always go to first col */
        break;

    case 'H': /* HTS -- Horizontal tab stop */
        term.tabs[term.c.x] = 1;
        break;

    case 'M': /* RI -- Reverse index */
        if (term.c.y == term.top)
            tscrolldown(term.top, 1, 1);
        else
            tmoveto(term.c.x, term.c.y - 1);
        break;

    case 'Z': /* DECID -- Identify Terminal */
        ttywrite(vtiden, 0);
        break;

    case 'c': /* RIS -- Reset to initial state */
        treset();
        xfreetitlestack();
        resettitle();
        xloadcols();
        break;

    case '=': /* DECPAM -- Application keypad */
        xsetmode(1, MODE_APPKEYPAD);
        break;

    case '>': /* DECPNM -- Normal keypad */
        xsetmode(0, MODE_APPKEYPAD);
        break;

    case '7': /* DECSC -- Save Cursor */
        tcursor(CURSOR_SAVE);
        break;

    case '8': /* DECRC -- Restore Cursor */
        tcursor(CURSOR_LOAD);
        break;

    case '\\': /* ST -- String Terminator */
        if (term.esc & ESC_STR_END)
            strhandle();
        break;

    default:
        fprintf(stderr, "erresc: unknown sequence ESC 0x%02X '%c'\n", (uchar)ascii, isprint(ascii) ? ascii : '.');
        break;
    }
    return 1;
}

void Con::readcolonargs(char** p, int cursor, std::array<std::array<int, ESC_ARG_SIZ>, CAR_PER_ARG>& params)
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