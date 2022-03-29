#include "con.hpp"
#include "../win.h"

extern TermWindow win;

int Con::tlinelen(int y)
{
    int i = term.col;

    if (TLINE(y)[i - 1].mode & ATTR_WRAP)
        return i;

    while (i > 0 && TLINE(y)[i - 1].u == ' ')
        --i;

    return i;
}

int Con::tattrset(int attr)
{
    int i, j;

    for (i = 0; i < term.row - 1; i++)
    {
        for (j = 0; j < term.col - 1; j++)
        {
            if (term.line[i][j].mode & attr)
                return 1;
        }
    }

    return 0;
}

void Con::tsetdirt(int top, int bot)
{
    int i;

    LIMIT(top, 0, term.row - 1);
    LIMIT(bot, 0, term.row - 1);

    for (i = top; i <= bot; i++)
        term.dirty[i] = 1;
}

void Con::tsetdirtattr(int attr)
{
    int i, j;

    for (i = 0; i < term.row - 1; i++)
    {
        for (j = 0; j < term.col - 1; j++)
        {
            if (term.line[i][j].mode & attr)
            {
                tsetdirt(i, i);
                break;
            }
        }
    }
}

void Con::tfulldirt(void)
{
    tsetdirt(0, term.row - 1);
}

void Con::tcursor(int mode)
{
    static TCursor c[2];
    int            alt = IS_SET(MODE_ALTSCREEN);

    if (mode == CURSOR_SAVE)
    {
        c[alt] = term.c;
    }
    else if (mode == CURSOR_LOAD)
    {
        term.c = c[alt];
        tmoveto(c[alt].x, c[alt].y);
    }
}

void Con::treset(void)
{
    uint i;

    term.c = TCursor{.attr = {.mode = ATTR_NULL, .fg = defaultfg, .bg = defaultbg}, .x = 0, .y = 0, .state = CURSOR_DEFAULT};

    memset(term.tabs.data(), 0, term.col * sizeof(term.tabs[0]));
    for (i = tabspaces; i < term.col; i += tabspaces)
        term.tabs[i] = 1;
    term.top  = 0;
    term.bot  = term.row - 1;
    term.mode = MODE_WRAP | MODE_UTF8;
    memset(term.trantbl.data(), CS_USA, sizeof(term.trantbl));
    term.charset = 0;

    for (i = 0; i < 2; i++)
    {
        tmoveto(0, 0);
        tcursor(CURSOR_SAVE);
        tclearregion(0, 0, term.col - 1, term.row - 1);
        tswapscreen();
    }
}

void Con::tnew(int col, int row)
{
    term = (Term){.c = {.attr = {.fg = defaultfg, .bg = defaultbg}}};
    tresize(col, row);
    treset();
}

void Con::tswapscreen(void)
{
    std::swap(term.line, term.alt);
    term.mode ^= MODE_ALTSCREEN;
    tfulldirt();
}

void Con::tscrolldown(int orig, int n, int copyhist)
{
    int  i;
    Line temp;

    LIMIT(n, 0, term.bot - orig + 1);

    if (copyhist)
    {
        term.histi            = (term.histi - 1 + HISTSIZE) % HISTSIZE;
        temp                  = term.hist[term.histi];
        term.hist[term.histi] = term.line[term.bot];
        term.line[term.bot]   = temp;
    }

    tsetdirt(orig, term.bot - n);
    tclearregion(0, term.bot - n + 1, term.col - 1, term.bot);

    for (i = term.bot; i >= orig + n; i--)
    {
        temp             = term.line[i];
        term.line[i]     = term.line[i - n];
        term.line[i - n] = temp;
    }

    if (term.scr == 0)
        selscroll(orig, n);
}

void Con::tscrollup(int orig, int n, int copyhist)
{
    int  i;
    Line temp;

    LIMIT(n, 0, term.bot - orig + 1);

    if (copyhist)
    {
        term.histi            = (term.histi + 1) % HISTSIZE;
        temp                  = term.hist[term.histi];
        term.hist[term.histi] = term.line[orig];
        term.line[orig]       = temp;
    }

    if (term.scr > 0 && term.scr < HISTSIZE)
        term.scr = MIN(term.scr + n, HISTSIZE - 1);

    tclearregion(0, orig, term.col - 1, orig + n - 1);
    tsetdirt(orig + n, term.bot);

    for (i = orig; i <= term.bot - n; i++)
    {
        temp             = term.line[i];
        term.line[i]     = term.line[i + n];
        term.line[i + n] = temp;
    }

    if (term.scr == 0)
        selscroll(orig, -n);
}

void Con::tnewline(int first_col)
{
    int y = term.c.y;

    if (y == term.bot)
    {
        tscrollup(term.top, 1, 1);
    }
    else
    {
        y++;
    }
    tmoveto(first_col ? 0 : term.c.x, y);
}

/* for absolute user moves, when decom is set */
void Con::tmoveato(int x, int y)
{
    tmoveto(x, y + ((term.c.state & CURSOR_ORIGIN) ? term.top : 0));
}

void Con::tmoveto(int x, int y)
{
    int miny, maxy;

    if (term.c.state & CURSOR_ORIGIN)
    {
        miny = term.top;
        maxy = term.bot;
    }
    else
    {
        miny = 0;
        maxy = term.row - 1;
    }
    term.c.state &= ~CURSOR_WRAPNEXT;
    term.c.x = LIMIT(x, 0, term.col - 1);
    term.c.y = LIMIT(y, miny, maxy);
}

void Con::tsetchar(Rune u, Glyph* attr, int x, int y)
{
    static char const* vt100_0[62] = {
        /* 0x41 - 0x7e */
        "↑", "↓", "→", "←", "█", "▚", "☃",      /* A - G */
        0,   0,   0,   0,   0,   0,   0,   0,   /* H - O */
        0,   0,   0,   0,   0,   0,   0,   0,   /* P - W */
        0,   0,   0,   0,   0,   0,   0,   " ", /* X - _ */
        "◆", "▒", "␉", "␌", "␍", "␊", "°", "±", /* ` - g */
        "␤", "␋", "┘", "┐", "┌", "└", "┼", "⎺", /* h - o */
        "⎻", "─", "⎼", "⎽", "├", "┤", "┴", "┬", /* p - w */
        "│", "≤", "≥", "π", "≠", "£", "·",      /* x - ~ */
    };

    /*
     * The table is proudly stolen from rxvt.
     */
    if (term.trantbl[term.charset] == CS_GRAPHIC0 && BETWEEN(u, 0x41, 0x7e) && vt100_0[u - 0x41])
        utf8decode(vt100_0[u - 0x41], u, UTF_SIZ);

    if (term.line[y][x].mode & ATTR_WIDE)
    {
        if (x + 1 < term.col)
        {
            term.line[y][x + 1].u = ' ';
            term.line[y][x + 1].mode &= ~ATTR_WDUMMY;
        }
    }
    else if (term.line[y][x].mode & ATTR_WDUMMY)
    {
        term.line[y][x - 1].u = ' ';
        term.line[y][x - 1].mode &= ~ATTR_WIDE;
    }

    term.dirty[y]     = 1;
    term.line[y][x]   = *attr;
    term.line[y][x].u = u;

    if (isboxdraw(u))
        term.line[y][x].mode |= ATTR_BOXDRAW;
}

void Con::tclearregion(int x1, int y1, int x2, int y2)
{
    int    x, y, temp;
    Glyph* gp;

    if (x1 > x2)
        temp = x1, x1 = x2, x2 = temp;
    if (y1 > y2)
        temp = y1, y1 = y2, y2 = temp;

    LIMIT(x1, 0, term.col - 1);
    LIMIT(x2, 0, term.col - 1);
    LIMIT(y1, 0, term.row - 1);
    LIMIT(y2, 0, term.row - 1);

    for (y = y1; y <= y2; y++)
    {
        term.dirty[y] = 1;
        for (x = x1; x <= x2; x++)
        {
            gp = &term.line[y][x];
            if (selected(x, y))
                selclear();
            gp->fg   = term.c.attr.fg;
            gp->bg   = term.c.attr.bg;
            gp->mode = 0;
            gp->u    = ' ';
        }
    }
}

void Con::tdeletechar(int n)
{
    LIMIT(n, 0, term.col - term.c.x);

    auto  dst  = term.c.x;
    auto  src  = term.c.x + n;
    auto  size = term.col - src;
    auto& line = term.line[term.c.y];

    memmove(&line[dst], &line[src], size * sizeof(Glyph));
    tclearregion(term.col - n, term.c.y, term.col - 1, term.c.y);
}

void Con::tinsertblank(int n)
{
    LIMIT(n, 0, term.col - term.c.x);

    auto  dst  = term.c.x + n;
    auto  src  = term.c.x;
    auto  size = term.col - dst;
    auto& line = term.line[term.c.y];

    memmove(&line[dst], &line[src], size * sizeof(Glyph));
    tclearregion(src, term.c.y, dst - 1, term.c.y);
}

void Con::tinsertblankline(int n)
{
    if (BETWEEN(term.c.y, term.top, term.bot))
        tscrolldown(term.c.y, n, 0);
}

void Con::tdeleteline(int n)
{
    if (BETWEEN(term.c.y, term.top, term.bot))
        tscrollup(term.c.y, n, 0);
}

int32_t Con::tdefcolor(int* attr, int* npar, int l)
{
    int32_t idx = -1;
    uint    r, g, b;

    switch (attr[*npar + 1])
    {
    case 2: /* direct color in RGB space */
        if (*npar + 4 >= l)
        {
            fprintf(stderr, "erresc(38): Incorrect number of parameters (%d)\n", *npar);
            break;
        }
        r = attr[*npar + 2];
        g = attr[*npar + 3];
        b = attr[*npar + 4];
        *npar += 4;
        if (!BETWEEN(r, 0, 255) || !BETWEEN(g, 0, 255) || !BETWEEN(b, 0, 255))
            fprintf(stderr, "erresc: bad rgb color (%u,%u,%u)\n", r, g, b);
        else
            idx = TRUECOLOR(r, g, b);
        break;
    case 5: /* indexed color */
        if (*npar + 2 >= l)
        {
            fprintf(stderr, "erresc(38): Incorrect number of parameters (%d)\n", *npar);
            break;
        }
        *npar += 2;
        if (!BETWEEN(attr[*npar], 0, 255))
            fprintf(stderr, "erresc: bad fgcolor %d\n", attr[*npar]);
        else
            idx = attr[*npar];
        break;
    case 0: /* implemented defined (only foreground) */
    case 1: /* transparent */
    case 3: /* direct color in CMY space */
    case 4: /* direct color in CMYK space */
    default:
        fprintf(stderr, "erresc(38): gfx attr %d unknown\n", attr[*npar]);
        break;
    }

    return idx;
}

void Con::tsetattr(int* attr, int l)
{
    int     i;
    int32_t idx;

    for (i = 0; i < l; i++)
    {
        switch (attr[i])
        {
        case 0:
            term.c.attr.mode &= ~(ATTR_BOLD | ATTR_FAINT | ATTR_ITALIC | ATTR_UNDERLINE | ATTR_BLINK | ATTR_REVERSE | ATTR_INVISIBLE | ATTR_STRUCK);
            term.c.attr.fg        = defaultfg;
            term.c.attr.bg        = defaultbg;
            term.c.attr.ustyle    = -1;
            term.c.attr.ucolor[0] = -1;
            term.c.attr.ucolor[1] = -1;
            term.c.attr.ucolor[2] = -1;
            break;
        case 1:
            term.c.attr.mode |= ATTR_BOLD;
            break;
        case 2:
            term.c.attr.mode |= ATTR_FAINT;
            break;
        case 3:
            term.c.attr.mode |= ATTR_ITALIC;
            break;
        case 4:
            term.c.attr.ustyle = csiescseq.carg[i][0];

            if (term.c.attr.ustyle != 0)
                term.c.attr.mode |= ATTR_UNDERLINE;
            else
                term.c.attr.mode &= ~ATTR_UNDERLINE;

            term.c.attr.mode ^= ATTR_DIRTYUNDERLINE;
            break;
        case 5: /* slow blink */
            [[fallthrough]];
        case 6: /* rapid blink */
            term.c.attr.mode |= ATTR_BLINK;
            break;
        case 7:
            term.c.attr.mode |= ATTR_REVERSE;
            break;
        case 8:
            term.c.attr.mode |= ATTR_INVISIBLE;
            break;
        case 9:
            term.c.attr.mode |= ATTR_STRUCK;
            break;
        case 22:
            term.c.attr.mode &= ~(ATTR_BOLD | ATTR_FAINT);
            break;
        case 23:
            term.c.attr.mode &= ~ATTR_ITALIC;
            break;
        case 24:
            term.c.attr.mode &= ~ATTR_UNDERLINE;
            break;
        case 25:
            term.c.attr.mode &= ~ATTR_BLINK;
            break;
        case 27:
            term.c.attr.mode &= ~ATTR_REVERSE;
            break;
        case 28:
            term.c.attr.mode &= ~ATTR_INVISIBLE;
            break;
        case 29:
            term.c.attr.mode &= ~ATTR_STRUCK;
            break;
        case 38:
            if ((idx = tdefcolor(attr, &i, l)) >= 0)
                term.c.attr.fg = idx;
            break;
        case 39:
            term.c.attr.fg = defaultfg;
            break;
        case 48:
            if ((idx = tdefcolor(attr, &i, l)) >= 0)
                term.c.attr.bg = idx;
            break;
        case 49:
            term.c.attr.bg = defaultbg;
            break;
        case 58:
            term.c.attr.ucolor[0] = csiescseq.carg[i][1];
            term.c.attr.ucolor[1] = csiescseq.carg[i][2];
            term.c.attr.ucolor[2] = csiescseq.carg[i][3];
            term.c.attr.mode ^= ATTR_DIRTYUNDERLINE;
            break;
        case 59:
            term.c.attr.ucolor[0] = -1;
            term.c.attr.ucolor[1] = -1;
            term.c.attr.ucolor[2] = -1;
            term.c.attr.mode ^= ATTR_DIRTYUNDERLINE;
            break;
        default:
            if (BETWEEN(attr[i], 30, 37))
            {
                term.c.attr.fg = attr[i] - 30;
            }
            else if (BETWEEN(attr[i], 40, 47))
            {
                term.c.attr.bg = attr[i] - 40;
            }
            else if (BETWEEN(attr[i], 90, 97))
            {
                term.c.attr.fg = attr[i] - 90 + 8;
            }
            else if (BETWEEN(attr[i], 100, 107))
            {
                term.c.attr.bg = attr[i] - 100 + 8;
            }
            else
            {
                fprintf(stderr, "erresc(default): gfx attr %d unknown\n", attr[i]);
                csidump();
            }
            break;
        }
    }
}

void Con::tsetscroll(int t, int b)
{
    int temp;

    LIMIT(t, 0, term.row - 1);
    LIMIT(b, 0, term.row - 1);
    if (t > b)
    {
        temp = t;
        t    = b;
        b    = temp;
    }
    term.top = t;
    term.bot = b;
}

void Con::tsetmode(int priv, int set, int* args, int narg)
{
    int alt, *lim;

    for (lim = args + narg; args < lim; ++args)
    {
        if (priv)
        {
            switch (*args)
            {
            case 1: /* DECCKM -- Cursor key */
                xsetmode(set, MODE_APPCURSOR);
                break;
            case 5: /* DECSCNM -- Reverse video */
                xsetmode(set, MODE_REVERSE);
                break;
            case 6: /* DECOM -- Origin */
                MODBIT(term.c.state, set, CURSOR_ORIGIN);
                tmoveato(0, 0);
                break;
            case 7: /* DECAWM -- Auto wrap */
                MODBIT(term.mode, set, MODE_WRAP);
                break;
            case 0:  /* Error (IGNORED) */
            case 2:  /* DECANM -- ANSI/VT52 (IGNORED) */
            case 3:  /* DECCOLM -- Column  (IGNORED) */
            case 4:  /* DECSCLM -- Scroll (IGNORED) */
            case 8:  /* DECARM -- Auto repeat (IGNORED) */
            case 18: /* DECPFF -- Printer feed (IGNORED) */
            case 19: /* DECPEX -- Printer extent (IGNORED) */
            case 42: /* DECNRCM -- National characters (IGNORED) */
            case 12: /* att610 -- Start blinking cursor (IGNORED) */
                break;
            case 25: /* DECTCEM -- Text Cursor Enable Mode */
                xsetmode(!set, MODE_HIDE);
                break;
            case 9: /* X10 mouse compatibility mode */
                xsetpointermotion(0);
                xsetmode(0, MODE_MOUSE);
                xsetmode(set, MODE_MOUSEX10);
                break;
            case 1000: /* 1000: report button press */
                xsetpointermotion(0);
                xsetmode(0, MODE_MOUSE);
                xsetmode(set, MODE_MOUSEBTN);
                break;
            case 1002: /* 1002: report motion on button press */
                xsetpointermotion(0);
                xsetmode(0, MODE_MOUSE);
                xsetmode(set, MODE_MOUSEMOTION);
                break;
            case 1003: /* 1003: enable all mouse motions */
                xsetpointermotion(set);
                xsetmode(0, MODE_MOUSE);
                xsetmode(set, MODE_MOUSEMANY);
                break;
            case 1004: /* 1004: send focus events to tty */
                xsetmode(set, MODE_FOCUS);
                break;
            case 1006: /* 1006: extended reporting mode */
                xsetmode(set, MODE_MOUSESGR);
                break;
            case 1034:
                xsetmode(set, MODE_8BIT);
                break;
            case 1049: /* swap screen & set/restore cursor as xterm */
                if (!allowaltscreen)
                    break;
                tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
                [[fallthrough]];
            case 47: /* swap screen */
            case 1047:
                if (!allowaltscreen)
                    break;
                alt = IS_SET(MODE_ALTSCREEN);
                if (alt)
                {
                    tclearregion(0, 0, term.col - 1, term.row - 1);
                }
                if (set ^ alt) /* set is always 1 or 0 */
                    tswapscreen();
                if (*args != 1049)
                    break;
                [[fallthrough]];
            case 1048:
                tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
                break;
            case 2004: /* 2004: bracketed paste mode */
                xsetmode(set, MODE_BRCKTPASTE);
                break;
            /* Not implemented mouse modes. See comments there. */
            case 1001: /* mouse highlight mode; can hang the
                      terminal by design when implemented. */
            case 1005: /* UTF-8 mouse mode; will confuse
                      applications not supporting UTF-8
                      and luit. */
            case 1015: /* urxvt mangled mouse mode; incompatible
                      and can be mistaken for other control
                      codes. */
                break;
            default:
                fprintf(stderr, "erresc: unknown private set/reset mode %d\n", *args);
                break;
            }
        }
        else
        {
            switch (*args)
            {
            case 0: /* Error (IGNORED) */
                break;
            case 2:
                xsetmode(set, MODE_KBDLOCK);
                break;
            case 4: /* IRM -- Insertion-replacement */
                MODBIT(term.mode, set, MODE_INSERT);
                break;
            case 12: /* SRM -- Send/Receive */
                MODBIT(term.mode, !set, MODE_ECHO);
                break;
            case 20: /* LNM -- Linefeed/new line */
                MODBIT(term.mode, set, MODE_CRLF);
                break;
            default:
                fprintf(stderr, "erresc: unknown set/reset mode %d\n", *args);
                break;
            }
        }
    }
}

void Con::tprinter(char const* s, size_t len)
{
    if (iofd != -1 && xwrite(iofd, s, len) < 0)
    {
        perror("Error writing to output file");
        close(iofd);
        iofd = -1;
    }
}
void Con::tdumpsel(void)
{
    char* ptr;

    if ((ptr = getsel()))
    {
        tprinter(ptr, strlen(ptr));
        free(ptr);
    }
}

void Con::tdumpline(int n)
{
    char   buf[UTF_SIZ];
    Glyph *bp, *end;

    bp  = &term.line[n][0];
    end = &bp[MIN(tlinelen(n), term.col) - 1];
    if (bp != end || bp->u != ' ')
    {
        for (; bp <= end; ++bp)
            tprinter(buf, utf8encode(bp->u, buf));
    }
    tprinter("\n", 1);
}

void Con::tdump(void)
{
    int i;

    for (i = 0; i < term.row; ++i)
        tdumpline(i);
}

void Con::tputtab(int n)
{
    uint x = term.c.x;

    if (n > 0)
    {
        while (x < term.col && n--)
            for (++x; x < term.col && !term.tabs[x]; ++x)
                /* nothing */;
    }
    else if (n < 0)
    {
        while (x > 0 && n++)
            for (--x; x > 0 && !term.tabs[x]; --x)
                /* nothing */;
    }
    term.c.x = LIMIT(x, 0, term.col - 1);
}

void Con::tdefutf8(char ascii)
{
    if (ascii == 'G')
        term.mode |= MODE_UTF8;
    else if (ascii == '@')
        term.mode &= ~MODE_UTF8;
}

void Con::tdeftran(char ascii)
{
    static char cs[]  = "0B";
    static int  vcs[] = {CS_GRAPHIC0, CS_USA};
    char*       p;

    if ((p = strchr(cs, ascii)) == NULL)
    {
        fprintf(stderr, "esc unhandled charset: ESC ( %c\n", ascii);
    }
    else
    {
        term.trantbl[term.icharset] = vcs[p - cs];
    }
}

void Con::tdectest(char c)
{
    int x, y;

    if (c == '8')
    { /* DEC screen alignment test. */
        for (x = 0; x < term.col; ++x)
        {
            for (y = 0; y < term.row; ++y)
                tsetchar('E', &term.c.attr, x, y);
        }
    }
}

void Con::tstrsequence(uchar c)
{
    strreset();

    switch (c)
    {
    case 0x90: /* DCS -- Device Control String */
        c = 'P';
        break;

    case 0x9f: /* APC -- Application Program Command */
        c = '_';
        break;

    case 0x9e: /* PM -- Privacy Message */
        c = '^';
        break;

    case 0x9d: /* OSC -- Operating System Command */
        c = ']';
        break;
    }

    strescseq.type = c;
    term.esc |= ESC_STR;
}

void Con::tcontrolcode(uchar ascii)
{
    switch (ascii)
    {
    case '\t': /* HT */
        tputtab(1);
        return;

    case '\b': /* BS */
        tmoveto(term.c.x - 1, term.c.y);
        return;

    case '\r': /* CR */
        tmoveto(0, term.c.y);
        return;

    case '\f': /* LF */
    case '\v': /* VT */
    case '\n': /* LF */
        /* go to first col if the mode is set */
        tnewline(IS_SET(MODE_CRLF));
        return;

    case '\a': /* BEL */
        if (term.esc & ESC_STR_END)
            /* backwards compatibility to xterm */
            strhandle();
        else
            xbell();
        break;

    case '\033': /* ESC */
        csireset();
        term.esc &= ~(ESC_CSI | ESC_ALTCHARSET | ESC_TEST);
        term.esc |= ESC_START;
        return;

    case '\016': /* SO (LS1 -- Locking shift 1) */
    case '\017': /* SI (LS0 -- Locking shift 0) */
        term.charset = 1 - (ascii - '\016');
        return;

    case '\032': /* SUB */
        tsetchar('?', &term.c.attr, term.c.x, term.c.y);
        [[fallthrough]];
    case '\030': /* CAN */
        csireset();
        break;

    case '\005': /* ENQ (IGNORED) */
    case '\000': /* NUL (IGNORED) */
    case '\021': /* XON (IGNORED) */
    case '\023': /* XOFF (IGNORED) */
    case 0177:   /* DEL (IGNORED) */
        return;

    case 0x80: /* TODO: PAD */
    case 0x81: /* TODO: HOP */
    case 0x82: /* TODO: BPH */
    case 0x83: /* TODO: NBH */
    case 0x84: /* TODO: IND */
        break;

    case 0x85:       /* NEL -- Next line */
        tnewline(1); /* always go to first col */
        break;

    case 0x86: /* TODO: SSA */
    case 0x87: /* TODO: ESA */
        break;

    case 0x88: /* HTS -- Horizontal tab stop */
        term.tabs[term.c.x] = 1;
        break;

    case 0x89: /* TODO: HTJ */
    case 0x8a: /* TODO: VTS */
    case 0x8b: /* TODO: PLD */
    case 0x8c: /* TODO: PLU */
    case 0x8d: /* TODO: RI */
    case 0x8e: /* TODO: SS2 */
    case 0x8f: /* TODO: SS3 */
    case 0x91: /* TODO: PU1 */
    case 0x92: /* TODO: PU2 */
    case 0x93: /* TODO: STS */
    case 0x94: /* TODO: CCH */
    case 0x95: /* TODO: MW */
    case 0x96: /* TODO: SPA */
    case 0x97: /* TODO: EPA */
    case 0x98: /* TODO: SOS */
    case 0x99: /* TODO: SGCI */
        break;

    case 0x9a: /* DECID -- Identify Terminal */
        ttywrite(vtiden, 0);
        break;

    case 0x9b: /* TODO: CSI */
    case 0x9c: /* TODO: ST */
        break;

    case 0x90: /* DCS -- Device Control String */
    case 0x9d: /* OSC -- Operating System Command */
    case 0x9e: /* PM -- Privacy Message */
    case 0x9f: /* APC -- Application Program Command */
        tstrsequence(ascii);
        return;
    }
    /* only CAN, SUB, \a and C1 chars interrupt a sequence */
    term.esc &= ~(ESC_STR_END | ESC_STR);
}

int Con::twrite(std::string_view buf, int show_ctrl)
{
    int  charsize;
    Rune u;
    int  n;

    int su0        = su;
    twrite_aborted = 0;

    for (n = 0; n < buf.size(); n += charsize)
    {
        if (IS_SET(MODE_UTF8) && !IS_SET(MODE_SIXEL))
        {
            /* process a complete utf8 char */
            charsize = utf8decode(buf.data() + n, u, buf.size() - n);
            if (charsize == 0)
                break;
        }
        else
        {
            u        = buf[n] & 0xFF;
            charsize = 1;
        }
        if (su0 && !su)
        {
            twrite_aborted = 1;
            break; // ESU - allow rendering before a new BSU
        }
        if (show_ctrl && ISCONTROL(u))
        {
            if (u & 0x80)
            {
                u &= 0x7f;
                tputc('^');
                tputc('[');
            }
            else if (u != '\n' && u != '\r' && u != '\t')
            {
                u ^= 0x40;
                tputc('^');
            }
        }
        tputc(u);
    }
    return n;
}

void Con::tresize(int col, int row)
{
    int     i, j;
    int     minrow = MIN(row, term.row);
    int     mincol = MIN(col, term.col);
    int*    bp;
    TCursor c;

    if (col < 1 || row < 1)
    {
        fprintf(stderr, "tresize: error resizing to %dx%d\n", col, row);
        return;
    }

    /*
     * slide screen to keep cursor where we expect it -
     * tscrollup would work here, but we can optimize to
     * memmove because we're freeing the earlier lines
     */
    for (i = 0; i <= term.c.y - row; i++)
    {
        term.line[i].~vector();
        term.alt[i].~vector();
    }
    /* ensure that both src and dst are not NULL */
    if (i > 0)
    {
        memmove(term.line.data(), term.line.data() + i, row * sizeof(Line));
        memmove(term.alt.data(), term.alt.data() + i, row * sizeof(Line));
    }

    /* resize to new height */
    term.line.resize(row);
    term.alt.resize(row);
    term.dirty.resize(row);
    term.tabs.resize(col);

    for (i = 0; i < HISTSIZE; i++)
    {
        term.hist[i].resize(col);
        for (j = mincol; j < col; j++)
        {
            term.hist[i][j]   = term.c.attr;
            term.hist[i][j].u = ' ';
        }
    }

    /* resize each row to new width, zero-pad if needed */
    for (i = 0; i < minrow; i++)
    {
        term.line[i].resize(col);
        term.alt[i].resize(col);
    }

    /* allocate any new rows */
    for (/* i = minrow */; i < row; i++)
    {
        term.line[i].resize(col);
        term.alt[i].resize(col);
    }
    if (col > term.col)
    {
        bp = term.tabs.data() + term.col;

        memset(bp, 0, sizeof(term.tabs[0]) * (col - term.col));
        while (--bp > term.tabs.data() && !*bp) {}
        for (bp += tabspaces; bp < term.tabs.data() + col; bp += tabspaces)
            *bp = 1;
    }
    /* update terminal size */
    term.col = col;
    term.row = row;
    /* reset scrolling region */
    tsetscroll(0, row - 1);
    /* make use of the LIMIT in tmoveto */
    tmoveto(term.c.x, term.c.y);
    /* Clearing both screens (it makes dirty all lines) */
    c = term.c;
    for (i = 0; i < 2; i++)
    {
        if (mincol < col && 0 < minrow)
        {
            tclearregion(mincol, 0, col - 1, minrow - 1);
        }
        if (0 < col && minrow < row)
        {
            tclearregion(0, minrow, col - 1, row - 1);
        }
        tswapscreen();
        tcursor(CURSOR_LOAD);
    }
    term.c = c;
}

#include "sixel.hpp"

void Con::tputc(Rune u)
{
    static std::string sixel;

    char   c[UTF_SIZ];
    int    control;
    int    width, len;
    Glyph* gp;

    control = ISCONTROL(u);
    if (u < 127 || !IS_SET(MODE_UTF8 | MODE_SIXEL))
    {
        c[0]  = u;
        width = len = 1;
    }
    else
    {
        len = utf8encode(u, c);
        if (!control && (width = wcwidth(u)) == -1)
            width = 1;
    }

    if (IS_SET(MODE_PRINT))
        tprinter(c, len);

    /*
     * STR sequence must be checked before anything else
     * because it uses all following characters until it
     * receives a ESC, a SUB, a ST or any other C1 control
     * character.
     */
    if (term.esc & ESC_STR)
    {
        if (u == '\a' || u == 030 || u == 032 || u == 033 || ISCONTROLC1(u))
        {
            if (IS_SET(MODE_SIXEL))
            {
                term.mode &= ~MODE_SIXEL;
                Image new_image{
                    .x = static_cast<size_t>(term.c.x),
                    .y = static_cast<size_t>(term.c.y),
                };
                std::tie(new_image.data, new_image.width, new_image.height) = parse_sixel(sixel);

                sixel.clear();
                term.images.push_back(new_image);

                for (size_t i = 0; i < (new_image.height + win.ch - 1) / win.ch; ++i)
                {
                    tclearregion(term.c.x, term.c.y, term.c.x + (new_image.width + win.cw - 1) / win.cw, term.c.y);
                    tnewline(1);
                }
                return;
            }
            term.esc &= ~(ESC_START | ESC_STR | ESC_DCS);
            term.esc |= ESC_STR_END;
            goto check_control_code;
        }

        if (IS_SET(MODE_SIXEL))
        {
            /* TODO: implement sixel mode */
            sixel += (char)u;
            return;
        }
        if ((term.esc & ESC_DCS) != 0 && strescseq.len == 0 && u == 'q')
            term.mode |= MODE_SIXEL;

        if (strescseq.len + len >= strescseq.siz)
        {
            /*
             * Here is a bug in terminals. If the user never sends
             * some code to stop the str or esc command, then st
             * will stop responding. But this is better than
             * silently failing with unknown characters. At least
             * then users will report back.
             *
             * In the case users ever get fixed, here is the code:
             */
            /*
             * term.esc = 0;
             * strhandle();
             */
            if (strescseq.siz > (SIZE_MAX - UTF_SIZ) / 2)
                return;
            strescseq.siz *= 2;
            strescseq.buf = (char*)xrealloc(strescseq.buf, strescseq.siz);
        }

        memmove(&strescseq.buf[strescseq.len], c, len);
        strescseq.len += len;
        return;
    }

check_control_code:
    /*
     * Actions of control codes must be performed as soon they arrive
     * because they can be embedded inside a control sequence, and
     * they must not cause conflicts with sequences.
     */
    if (control)
    {
        tcontrolcode(u);
        /*
         * control codes are not shown ever
         */
        if (!term.esc)
            term.lastc = 0;
        return;
    }
    else if (term.esc & ESC_START)
    {
        if (term.esc & ESC_CSI)
        {
            csiescseq.buf[csiescseq.len++] = u;
            if (BETWEEN(u, 0x40, 0x7E) || csiescseq.len >= sizeof(csiescseq.buf) - 1)
            {
                term.esc = 0;
                csiparse();
                csihandle();
            }
            return;
        }
        else if (term.esc & ESC_UTF8)
        {
            tdefutf8(u);
        }
        else if (term.esc & ESC_ALTCHARSET)
        {
            tdeftran(u);
        }
        else if (term.esc & ESC_TEST)
        {
            tdectest(u);
        }
        else
        {
            if (!eschandle(u))
                return;
            /* sequence already finished */
        }
        term.esc = 0;
        /*
         * All characters which form part of a sequence are not
         * printed
         */
        return;
    }
    if (selected(term.c.x, term.c.y))
        selclear();

    gp = &term.line[term.c.y][term.c.x];
    if (IS_SET(MODE_WRAP) && (term.c.state & CURSOR_WRAPNEXT))
    {
        gp->mode |= ATTR_WRAP;
        tnewline(1);
        gp = &term.line[term.c.y][term.c.x];
    }

    if (IS_SET(MODE_INSERT) && term.c.x + width < term.col)
        memmove(gp + width, gp, (term.col - term.c.x - width) * sizeof(Glyph));

    if (term.c.x + width > term.col)
    {
        tnewline(1);
        gp = &term.line[term.c.y][term.c.x];
    }

    tsetchar(u, &term.c.attr, term.c.x, term.c.y);
    term.lastc = u;

    if (width == 2)
    {
        gp->mode |= ATTR_WIDE;
        if (term.c.x + 1 < term.col)
        {
            gp[1].u    = '\0';
            gp[1].mode = ATTR_WDUMMY;
        }
    }
    if (term.c.x + width < term.col)
    {
        tmoveto(term.c.x + width, term.c.y);
    }
    else
    {
        term.c.state |= CURSOR_WRAPNEXT;
    }
}