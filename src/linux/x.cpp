/* See LICENSE for license details. */
#define Glyph Glyph_
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>
#include <X11/Xcursor/Xcursor.h>
#undef Glyph

/* config.h for applying patches and the configuration. */
#include "config.hpp"

char* argv0;
#include "arg.h"
#include "st.h"
#include "win.h"
#include "hb_.h"

#include "con/con.hpp"
#include "time.hpp"

#include <array>

/* Undercurl slope types */
enum undercurl_slope_type
{
    UNDERCURL_SLOPE_ASCENDING  = 0,
    UNDERCURL_SLOPE_TOP_CAP    = 1,
    UNDERCURL_SLOPE_DESCENDING = 2,
    UNDERCURL_SLOPE_BOTTOM_CAP = 3
};

/* X modifiers */
#define XK_ANY_MOD UINT_MAX
#define XK_NO_MOD 0
#define XK_SWITCH_MOD (1 << 13)

#undef IS_SET

/* size of title stack */
#define TITLESTACKSIZE 8

/* XEMBED messages */
#define XEMBED_FOCUS_IN 4
#define XEMBED_FOCUS_OUT 5

/* macros */
#define IS_SET(flag) ((win.mode & (flag)) != 0)
#define TRUERED(x) (((x)&0xff0000) >> 8)
#define TRUEGREEN(x) (((x)&0xff00))
#define TRUEBLUE(x) (((x)&0xff) << 8)

typedef XftDraw*         Draw;
typedef XftColor         Color;
typedef XftGlyphFontSpec GlyphFontSpec;

typedef struct
{
    Display*       dpy;
    Colormap       cmap;
    Window         win;
    Drawable       buf;
    GlyphFontSpec* specbuf; /* font spec buffer used for rendering */
    Atom           xembed, wmdeletewin, netwmname, netwmpid, blur;
    struct
    {
        XIM           xim;
        XIC           xic;
        XPoint        spot;
        XVaNestedList spotlist;
    } ime;
    Draw                 draw;
    Visual*              vis;
    XSetWindowAttributes attrs;
    int                  scr;
    int                  isfixed; /* is fixed geometry? */
    int                  depth;   /* bit depth */
    int                  l, t;    /* left and top offset */
    int                  gm;      /* geometry mask */
} XWindow;

typedef struct
{
    Atom            xtarget;
    char *          primary, *clipboard;
    struct timespec tclick1;
    struct timespec tclick2;
} XSelection;

/* Font structure */
#define Font Font_
typedef struct
{
    int        height;
    int        width;
    int        ascent;
    int        descent;
    int        badslant;
    int        badweight;
    short      lbearing;
    short      rbearing;
    XftFont*   match;
    FcFontSet* set;
    FcPattern* pattern;
} Font;

/* Drawing Context */
typedef struct
{
    std::vector<Color> col;
    Font   font, bfont, ifont, ibfont;
    GC     gc;
} DC;

static inline ushort sixd_to_16bit(int);
static int           xmakeglyphfontspecs(XftGlyphFontSpec*, Glyph const*, int, int, int);
static void          xdrawglyphfontspecs(XftGlyphFontSpec const*, Glyph, int, int, int);
static void          xdrawglyph(Glyph, int, int);
static void          xclear(int, int, int, int);
static int           xgeommasktogravity(int);
static int           ximopen(Display*);
static void          ximinstantiate(Display*, XPointer, XPointer);
static void          ximdestroy(XIM, XPointer, XPointer);
static int           xicdestroy(XIC, XPointer, XPointer);
static void          xinit(int, int);
static void          cresize(int, int);
static void          xresize(int, int);
static void          xhints(void);
static int           xloadcolor(int, char const*, Color*);
static int           xloadfont(Font*, FcPattern*);
static void          xloadfonts(char*, double);
static void          xunloadfont(Font*);
static void          xunloadfonts(void);
static void          xsetenv(void);
static void          xseturgency(int);
static int           evcol(XEvent*);
static int           evrow(XEvent*);

static void        expose(XEvent*);
static void        visibility(XEvent*);
static void        unmap(XEvent*);
static void        kpress(XEvent*);
static void        cmessage(XEvent*);
static void        resize(XEvent*);
static void        focus(XEvent*);
static uint        buttonmask(uint);
static int         mouseaction(XEvent*, uint);
static void        brelease(XEvent*);
static void        bpress(XEvent*);
static void        bmotion(XEvent*);
static void        propnotify(XEvent*);
static void        selnotify(XEvent*);
static void        selclear_(XEvent*);
static void        selrequest(XEvent*);
static void        setsel(char*, Time);
static void        mousesel(XEvent*, int);
static void        mousereport(XEvent*);
static char const* kmap(KeySym, uint);
static int         match(uint, uint);

static void run(void);
static void usage(void);

static std::array<std::function<void(XEvent*)>, LASTEvent> handler = {
    // clang-format off
    nullptr, nullptr,
    /*[KeyPress]         =*/kpress,
    nullptr,
    /*[ButtonPress]      =*/bpress,
    /*[ButtonRelease]    =*/brelease,
    /*[MotionNotify]     =*/bmotion,
    nullptr,
    nullptr,
    /*[FocusIn]          =*/focus,
    /*[FocusOut]         =*/focus,
    nullptr,
    /*[Expose]           =*/expose,
    nullptr, nullptr,
    /*[VisibilityNotify] =*/visibility,
    nullptr, nullptr,
    /*[UnmapNotify]      =*/unmap,
    nullptr, nullptr, nullptr,
    /*[ConfigureNotify]  =*/resize,
    nullptr, nullptr, nullptr, nullptr, nullptr,
    /*[PropertyNotify]   =*/propnotify,
    /*[SelectionClear]   =*/selclear_,
    /*[SelectionRequest] =*/selrequest,
    /*[SelectionNotify]  =*/selnotify,
    nullptr,
    /*[ClientMessage]    =*/cmessage,
    // clang-format on
};

/* Globals */
static DC         dc;
static XWindow    xw;
static XSelection xsel;
TermWindow        win;
Con               con;
static int        tstki;                      /* title stack index */
static char*      titlestack[TITLESTACKSIZE]; /* title stack */

/* Font Ring Cache */
enum
{
    FRC_NORMAL,
    FRC_ITALIC,
    FRC_BOLD,
    FRC_ITALICBOLD
};

typedef struct
{
    XftFont* font;
    int      flags;
    Rune     unicodep;
} Fontcache;

/* Fontcache is an array now. A new font will be appended to the array. */
static Fontcache*  frc             = NULL;
static int         frclen          = 0;
static int         frccap          = 0;
static char const* usedfont        = NULL;
static double      usedfontsize    = 0;
static double      defaultfontsize = 0;

static char*  opt_alpha = NULL;
static char*  opt_class = NULL;
static char** opt_cmd   = NULL;
static char*  opt_embed = NULL;
static char*  opt_font  = NULL;
static char*  opt_io    = NULL;
static char*  opt_line  = NULL;
static char*  opt_name  = NULL;
static char*  opt_title = NULL;

static int oldbutton = 3; /* button event on startup: 3 = release */

static Cursor cursor;
static XColor xmousefg, xmousebg;

void clipcopy(Arg const& dummy)
{
    Atom clipboard;

    free(xsel.clipboard);
    xsel.clipboard = NULL;

    if (xsel.primary != NULL)
    {
        xsel.clipboard = xstrdup(xsel.primary);
        clipboard      = XInternAtom(xw.dpy, "CLIPBOARD", 0);
        XSetSelectionOwner(xw.dpy, clipboard, xw.win, CurrentTime);
    }
}

void clippaste(Arg const& dummy)
{
    Atom clipboard;

    clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
    XConvertSelection(xw.dpy, clipboard, xsel.xtarget, clipboard, xw.win, CurrentTime);
}

void selpaste(Arg const& dummy)
{
    XConvertSelection(xw.dpy, XA_PRIMARY, xsel.xtarget, XA_PRIMARY, xw.win, CurrentTime);
}

void numlock(Arg const& dummy)
{
    win.mode ^= MODE_NUMLOCK;
}

void zoom(Arg const& arg)
{
    Arg larg;

    larg = usedfontsize + std::get<double>(arg);
    zoomabs(larg);
}

void zoomabs(Arg const& arg)
{
    xunloadfonts();
    xloadfonts((char*)usedfont, std::get<double>(arg));
    cresize(0, 0);
    redraw();
    xhints();
}

void zoomreset(Arg const& arg)
{
    Arg larg;

    if (defaultfontsize > 0)
    {
        larg = defaultfontsize;
        zoomabs(larg);
    }
}

void ttysend(Arg const& arg)
{
    auto s = std::get<const char*>(arg);
    con.ttywrite({s, strlen(s)}, 1);
}

int evcol(XEvent* e)
{
    int x = e->xbutton.x - borderpx - win.hborderpx;
    LIMIT(x, 0, win.tw - 1);
    return x / win.cw;
}

int evrow(XEvent* e)
{
    int y = e->xbutton.y - borderpx - win.vborderpx;
    LIMIT(y, 0, win.th - 1);
    return y / win.ch;
}

void mousesel(XEvent* e, int done)
{
    int  type, seltype = SEL_REGULAR;
    uint state = e->xbutton.state & ~(Button1Mask | ShiftMask);

    for (type = 1; type < LEN(selmasks); ++type)
    {
        if (match(ljh::underlying_cast(selmasks[type]), state))
        {
            seltype = type;
            break;
        }
    }
    con.selextend(evcol(e), evrow(e), seltype, done);
    if (done)
        setsel(con.getsel(), e->xbutton.time);
}

void mousereport(XEvent* e)
{
    int        len, x = evcol(e), y = evrow(e), button = e->xbutton.button, state = e->xbutton.state;
    char       buf[40];
    static int ox, oy;

    /* from urxvt */
    if (e->xbutton.type == MotionNotify)
    {
        if (x == ox && y == oy)
            return;
        if (!IS_SET(MODE_MOUSEMOTION) && !IS_SET(MODE_MOUSEMANY))
            return;
        /* MOUSE_MOTION: no reporting if no button is pressed */
        if (IS_SET(MODE_MOUSEMOTION) && oldbutton == 3)
            return;

        button = oldbutton + 32;
        ox     = x;
        oy     = y;
    }
    else
    {
        if (!IS_SET(MODE_MOUSESGR) && e->xbutton.type == ButtonRelease)
        {
            button = 3;
        }
        else
        {
            button -= Button1;
            if (button >= 3)
                button += 64 - 3;
        }
        if (e->xbutton.type == ButtonPress)
        {
            oldbutton = button;
            ox        = x;
            oy        = y;
        }
        else if (e->xbutton.type == ButtonRelease)
        {
            oldbutton = 3;
            /* MODE_MOUSEX10: no button release reporting */
            if (IS_SET(MODE_MOUSEX10))
                return;
            if (button == 64 || button == 65)
                return;
        }
    }

    if (!IS_SET(MODE_MOUSEX10))
    {
        button += ((state & ShiftMask) ? 4 : 0) + ((state & Mod4Mask) ? 8 : 0) + ((state & ControlMask) ? 16 : 0);
    }

    if (IS_SET(MODE_MOUSESGR))
    {
        len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c", button, x + 1, y + 1, e->xbutton.type == ButtonRelease ? 'm' : 'M');
    }
    else if (x < 223 && y < 223)
    {
        len = snprintf(buf, sizeof(buf), "\033[M%c%c%c", 32 + button, 32 + x + 1, 32 + y + 1);
    }
    else
    {
        return;
    }

    con.ttywrite({buf, (size_t)len}, 0);
}

uint buttonmask(uint button)
{
    return button == Button1 ? Button1Mask
         : button == Button2 ? Button2Mask
         : button == Button3 ? Button3Mask
         : button == Button4 ? Button4Mask
         : button == Button5 ? Button5Mask
                             : 0;
}

int mouseaction(XEvent* e, uint release)
{
    MouseShortcut* ms;

    /* ignore Button<N>mask for Button<N> - it's set on release */
    uint state = e->xbutton.state & ~buttonmask(e->xbutton.button);

    for (ms = mshortcuts; ms < mshortcuts + LEN(mshortcuts); ms++)
    {
        if (ms->release == release && ljh::underlying_cast(ms->button) == e->xbutton.button &&
            (match(ljh::underlying_cast(ms->mod), state) || /* exact or forced */
             match(ljh::underlying_cast(ms->mod), state & ~ShiftMask)))
        {
            ms->func(ms->arg);
            return 1;
        }
    }

    return 0;
}

void bpress(XEvent* e)
{
    struct timespec now;
    int             snap;

    if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & ShiftMask))
    {
        mousereport(e);
        return;
    }

    if (mouseaction(e, 0))
        return;

    if (e->xbutton.button == Button1)
    {
        /*
         * If the user clicks below predefined timeouts specific
         * snapping behaviour is exposed.
         */
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (TIMEDIFF(now, xsel.tclick2) <= tripleclicktimeout)
        {
            snap = SNAP_LINE;
        }
        else if (TIMEDIFF(now, xsel.tclick1) <= doubleclicktimeout)
        {
            snap = SNAP_WORD;
        }
        else
        {
            snap = 0;
        }
        xsel.tclick2 = xsel.tclick1;
        xsel.tclick1 = now;

        con.selstart(evcol(e), evrow(e), snap);
    }
}

void propnotify(XEvent* e)
{
    XPropertyEvent* xpev;
    Atom            clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);

    xpev = &e->xproperty;
    if (xpev->state == PropertyNewValue && (xpev->atom == XA_PRIMARY || xpev->atom == clipboard))
    {
        selnotify(e);
    }
}

void selnotify(XEvent* e)
{
    ulong  nitems, ofs, rem;
    int    format;
    uchar *data, *last, *repl;
    Atom   type, incratom, property = 0;

    incratom = XInternAtom(xw.dpy, "INCR", 0);

    ofs = 0;
    if (e->type == SelectionNotify)
        property = e->xselection.property;
    else if (e->type == PropertyNotify)
        property = e->xproperty.atom;

    if (property == 0)
        return;

    do
    {
        if (XGetWindowProperty(xw.dpy, xw.win, property, ofs, BUFSIZ / 4, False, AnyPropertyType, &type, &format, &nitems, &rem, &data))
        {
            fprintf(stderr, "Clipboard allocation failed\n");
            return;
        }

        if (e->type == PropertyNotify && nitems == 0 && rem == 0)
        {
            /*
             * If there is some PropertyNotify with no data, then
             * this is the signal of the selection owner that all
             * data has been transferred. We won't need to receive
             * PropertyNotify events anymore.
             */
            MODBIT(xw.attrs.event_mask, 0, PropertyChangeMask);
            XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask, &xw.attrs);
        }

        if (type == incratom)
        {
            /*
             * Activate the PropertyNotify events so we receive
             * when the selection owner does send us the next
             * chunk of data.
             */
            MODBIT(xw.attrs.event_mask, 1, PropertyChangeMask);
            XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask, &xw.attrs);

            /*
             * Deleting the property is the transfer start signal.
             */
            XDeleteProperty(xw.dpy, xw.win, (int)property);
            continue;
        }

        /*
         * As seen in getsel:
         * Line endings are inconsistent in the terminal and GUI world
         * copy and pasting. When receiving some selection data,
         * replace all '\n' with '\r'.
         * FIXME: Fix the computer world.
         */
        repl = data;
        last = data + nitems * format / 8;
        while ((repl = (uchar*)memchr(repl, '\n', last - repl)))
        {
            *repl++ = '\r';
        }

        if (IS_SET(MODE_BRCKTPASTE) && ofs == 0)
            con.ttywrite("\033[200~", 0);
        con.ttywrite({(char*)data, nitems * format / 8}, 1);
        if (IS_SET(MODE_BRCKTPASTE) && rem == 0)
            con.ttywrite("\033[201~", 0);
        XFree(data);
        /* number of 32-bit chunks returned */
        ofs += nitems * format / 32;
    }
    while (rem > 0);

    /*
     * Deleting the property again tells the selection owner to send the
     * next data chunk in the property.
     */
    XDeleteProperty(xw.dpy, xw.win, (int)property);
}

void xclipcopy(void)
{
    clipcopy(0);
}

void selclear_(XEvent* e)
{
    con.selclear();
}

void selrequest(XEvent* e)
{
    XSelectionRequestEvent* xsre;
    XSelectionEvent         xev;
    Atom                    xa_targets, string, clipboard;
    char*                   seltext;

    xsre          = (XSelectionRequestEvent*)e;
    xev.type      = SelectionNotify;
    xev.requestor = xsre->requestor;
    xev.selection = xsre->selection;
    xev.target    = xsre->target;
    xev.time      = xsre->time;
    if (xsre->property == 0)
        xsre->property = xsre->target;

    /* reject */
    xev.property = 0;

    xa_targets = XInternAtom(xw.dpy, "TARGETS", 0);
    if (xsre->target == xa_targets)
    {
        /* respond with the supported type */
        string = xsel.xtarget;
        XChangeProperty(xsre->display, xsre->requestor, xsre->property, XA_ATOM, 32, PropModeReplace, (uchar*)&string, 1);
        xev.property = xsre->property;
    }
    else if (xsre->target == xsel.xtarget || xsre->target == XA_STRING)
    {
        /*
         * xith XA_STRING non ascii characters may be incorrect in the
         * requestor. It is not our problem, use utf8.
         */
        clipboard = XInternAtom(xw.dpy, "CLIPBOARD", 0);
        if (xsre->selection == XA_PRIMARY)
        {
            seltext = xsel.primary;
        }
        else if (xsre->selection == clipboard)
        {
            seltext = xsel.clipboard;
        }
        else
        {
            fprintf(stderr, "Unhandled clipboard selection 0x%lx\n", xsre->selection);
            return;
        }
        if (seltext != NULL)
        {
            XChangeProperty(xsre->display, xsre->requestor, xsre->property, xsre->target, 8, PropModeReplace, (uchar*)seltext, strlen(seltext));
            xev.property = xsre->property;
        }
    }

    /* all done, send a notification to the listener */
    if (!XSendEvent(xsre->display, xsre->requestor, 1, 0, (XEvent*)&xev))
        fprintf(stderr, "Error sending SelectionNotify event\n");
}

void setsel(char* str, Time t)
{
    if (!str)
        return;

    free(xsel.primary);
    xsel.primary = str;

    XSetSelectionOwner(xw.dpy, XA_PRIMARY, xw.win, t);
    if (XGetSelectionOwner(xw.dpy, XA_PRIMARY) != xw.win)
        con.selclear();
}

void xsetsel(char* str)
{
    setsel(str, CurrentTime);
}

void brelease(XEvent* e)
{
    if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & ShiftMask))
    {
        mousereport(e);
        return;
    }

    if (mouseaction(e, 1))
        return;
    if (e->xbutton.button == Button1)
        mousesel(e, 1);
}

void bmotion(XEvent* e)
{
    if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & ShiftMask))
    {
        mousereport(e);
        return;
    }

    mousesel(e, 0);
}

void cresize(int width, int height)
{
    int col, row;

    if (width != 0)
        win.w = width;
    if (height != 0)
        win.h = height;

    col = (win.w - 2 * borderpx) / win.cw;
    row = (win.h - 2 * borderpx) / win.ch;
    col = MAX(1, col);
    row = MAX(1, row);

    win.hborderpx = (win.w - col * win.cw) / 2;
    win.vborderpx = (win.h - row * win.ch) / 2;

    con.tresize(col, row);
    xresize(col, row);
    con.ttyresize(win.tw, win.th);
}

void xresize(int col, int row)
{
    win.tw = col * win.cw;
    win.th = row * win.ch;

    XFreePixmap(xw.dpy, xw.buf);
    xw.buf = XCreatePixmap(xw.dpy, xw.win, win.w, win.h, xw.depth);
    XftDrawChange(xw.draw, xw.buf);
    xclear(0, 0, win.w, win.h);

    /* resize to new width */
    xw.specbuf = (GlyphFontSpec*)xrealloc(xw.specbuf, col * sizeof(GlyphFontSpec));
}

ushort sixd_to_16bit(int x)
{
    return x == 0 ? 0 : 0x3737 + 0x2828 * x;
}

int xloadcolor(int i, char const* name, Color* ncolor)
{
    XRenderColor color = {.alpha = 0xffff};

    if (!name)
    {
        if (BETWEEN(i, 16, 255))
        { /* 256 color */
            if (i < 6 * 6 * 6 + 16)
            { /* same colors as xterm */
                color.red   = sixd_to_16bit(((i - 16) / 36) % 6);
                color.green = sixd_to_16bit(((i - 16) / 6) % 6);
                color.blue  = sixd_to_16bit(((i - 16) / 1) % 6);
            }
            else
            { /* greyscale */
                color.red   = 0x0808 + 0x0a0a * (i - (6 * 6 * 6 + 16));
                color.green = color.blue = color.red;
            }
            return XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &color, ncolor);
        }
        else
            name = colorname[i];
    }

    return XftColorAllocName(xw.dpy, xw.vis, xw.cmap, name, ncolor);
}

void xloadcols(void)
{
    int        i;
    static int loaded;
    Color*     cp;

    if (loaded)
    {
        for (auto& cp : dc.col)
            XftColorFree(xw.dpy, xw.vis, xw.cmap, &cp);
    }
    else
    {
        constexpr auto col_size = std::max(LEN(colorname), (size_t)256);
        dc.col.resize(col_size);
    }

    for (i = 0; i < dc.col.size(); i++)
        if (!xloadcolor(i, NULL, &dc.col[i]))
        {
            if (colorname[i])
                die("could not allocate color '%s'\n", colorname[i]);
            else
                die("could not allocate color %d\n", i);
        }

    /* set alpha value of bg color */
    if (opt_alpha)
        alpha = strtof(opt_alpha, NULL);
    dc.col[defaultbg].color.alpha = (unsigned short)(0xffff * alpha);
    dc.col[defaultbg].pixel &= 0x00FFFFFF;
    dc.col[defaultbg].pixel |= (unsigned char)(0xff * alpha) << 24;
    loaded = 1;
}

int xsetcolorname(int x, char const* name)
{
    Color ncolor;

    if (!BETWEEN(x, 0, dc.col.size()))
        return 1;

    if (!xloadcolor(x, name, &ncolor))
        return 1;

    XftColorFree(xw.dpy, xw.vis, xw.cmap, &dc.col[x]);
    dc.col[x] = ncolor;

    return 0;
}

/*
 * Absolute coordinates.
 */
void xclear(int x1, int y1, int x2, int y2)
{
    XftDrawRect(xw.draw, &dc.col[IS_SET(MODE_REVERSE) ? defaultfg : defaultbg], x1, y1, x2 - x1, y2 - y1);
}

void xhints(void)
{
    XClassHint  class_ = {(char*)(opt_name ? opt_name : termname), (char*)(opt_class ? opt_class : termname)};
    XWMHints    wm     = {.flags = InputHint, .input = 1};
    XSizeHints* sizeh;

    sizeh = XAllocSizeHints();

    sizeh->flags       = PSize | PResizeInc | PBaseSize | PMinSize;
    sizeh->height      = win.h;
    sizeh->width       = win.w;
    sizeh->height_inc  = 1;
    sizeh->width_inc   = 1;
    sizeh->base_height = 2 * borderpx;
    sizeh->base_width  = 2 * borderpx;
    sizeh->min_height  = win.ch + 2 * borderpx;
    sizeh->min_width   = win.cw + 2 * borderpx;
    if (xw.isfixed)
    {
        sizeh->flags |= PMaxSize;
        sizeh->min_width = sizeh->max_width = win.w;
        sizeh->min_height = sizeh->max_height = win.h;
    }
    if (xw.gm & (XValue | YValue))
    {
        sizeh->flags |= USPosition | PWinGravity;
        sizeh->x           = xw.l;
        sizeh->y           = xw.t;
        sizeh->win_gravity = xgeommasktogravity(xw.gm);
    }

    XSetWMProperties(xw.dpy, xw.win, NULL, NULL, NULL, 0, sizeh, &wm, &class_);
    XFree(sizeh);
}

int xgeommasktogravity(int mask)
{
    switch (mask & (XNegative | YNegative))
    {
    case 0:
        return NorthWestGravity;
    case XNegative:
        return NorthEastGravity;
    case YNegative:
        return SouthWestGravity;
    }

    return SouthEastGravity;
}

int xloadfont(Font* f, FcPattern* pattern)
{
    FcPattern* configured;
    FcPattern* match;
    FcResult   result;
    XGlyphInfo extents;
    int        wantattr, haveattr;

    /*
     * Manually configure instead of calling XftMatchFont
     * so that we can use the configured pattern for
     * "missing glyph" lookups.
     */
    configured = FcPatternDuplicate(pattern);
    if (!configured)
        return 1;

    FcConfigSubstitute(NULL, configured, FcMatchPattern);
    XftDefaultSubstitute(xw.dpy, xw.scr, configured);

    match = FcFontMatch(NULL, configured, &result);
    if (!match)
    {
        FcPatternDestroy(configured);
        return 1;
    }

    if (!(f->match = XftFontOpenPattern(xw.dpy, match)))
    {
        FcPatternDestroy(configured);
        FcPatternDestroy(match);
        return 1;
    }

    if ((XftPatternGetInteger(pattern, "slant", 0, &wantattr) == XftResultMatch))
    {
        /*
         * Check if xft was unable to find a font with the appropriate
         * slant but gave us one anyway. Try to mitigate.
         */
        if ((XftPatternGetInteger(f->match->pattern, "slant", 0, &haveattr) != XftResultMatch) || haveattr < wantattr)
        {
            f->badslant = 1;
            fputs("font slant does not match\n", stderr);
        }
    }

    if ((XftPatternGetInteger(pattern, "weight", 0, &wantattr) == XftResultMatch))
    {
        if ((XftPatternGetInteger(f->match->pattern, "weight", 0, &haveattr) != XftResultMatch) || haveattr != wantattr)
        {
            f->badweight = 1;
            fputs("font weight does not match\n", stderr);
        }
    }

    XftTextExtentsUtf8(xw.dpy, f->match, (FcChar8 const*)ascii_printable, strlen(ascii_printable), &extents);

    f->set     = NULL;
    f->pattern = configured;

    f->ascent   = f->match->ascent;
    f->descent  = f->match->descent;
    f->lbearing = 0;
    f->rbearing = f->match->max_advance_width;

    f->height = f->ascent + f->descent;
    f->width  = DIVCEIL(extents.xOff, strlen(ascii_printable));

    return 0;
}

void xloadfonts(char* fontstr, double fontsize)
{
    FcPattern* pattern;
    double     fontval;

    if (fontstr[0] == '-')
        pattern = XftXlfdParse(fontstr, False, False);
    else
        pattern = FcNameParse((FcChar8*)fontstr);

    if (!pattern)
        die("can't open font %s\n", fontstr);

    if (fontsize > 1)
    {
        FcPatternDel(pattern, FC_PIXEL_SIZE);
        FcPatternDel(pattern, FC_SIZE);
        FcPatternAddDouble(pattern, FC_PIXEL_SIZE, (double)fontsize);
        usedfontsize = fontsize;
    }
    else
    {
        if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &fontval) == FcResultMatch)
        {
            usedfontsize = fontval;
        }
        else if (FcPatternGetDouble(pattern, FC_SIZE, 0, &fontval) == FcResultMatch)
        {
            usedfontsize = -1;
        }
        else
        {
            /*
             * Default font size is 12, if none given. This is to
             * have a known usedfontsize value.
             */
            FcPatternAddDouble(pattern, FC_PIXEL_SIZE, 12);
            usedfontsize = 12;
        }
        defaultfontsize = usedfontsize;
    }

    if (xloadfont(&dc.font, pattern))
        die("can't open font %s\n", fontstr);

    if (usedfontsize < 0)
    {
        FcPatternGetDouble(dc.font.match->pattern, FC_PIXEL_SIZE, 0, &fontval);
        usedfontsize = fontval;
        if (fontsize == 0)
            defaultfontsize = fontval;
    }

    /* Setting character width and height. */
    win.cw = ceilf(dc.font.width * cwscale);
    win.ch = ceilf(dc.font.height * chscale);

    FcPatternDel(pattern, FC_SLANT);
    FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
    if (xloadfont(&dc.ifont, pattern))
        die("can't open font %s\n", fontstr);

    FcPatternDel(pattern, FC_WEIGHT);
    FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
    if (xloadfont(&dc.ibfont, pattern))
        die("can't open font %s\n", fontstr);

    FcPatternDel(pattern, FC_SLANT);
    FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
    if (xloadfont(&dc.bfont, pattern))
        die("can't open font %s\n", fontstr);

    FcPatternDestroy(pattern);
}

void xunloadfont(Font* f)
{
    XftFontClose(xw.dpy, f->match);
    FcPatternDestroy(f->pattern);
    if (f->set)
        FcFontSetDestroy(f->set);
}

void xunloadfonts(void)
{
    /* Clear Harfbuzz font cache. */
    hbunloadfonts();

    /* Free the loaded fonts in the font cache.  */
    while (frclen > 0)
        XftFontClose(xw.dpy, frc[--frclen].font);

    xunloadfont(&dc.font);
    xunloadfont(&dc.bfont);
    xunloadfont(&dc.ifont);
    xunloadfont(&dc.ibfont);
}

int ximopen(Display* dpy)
{
    XIMCallback imdestroy = {.client_data = NULL, .callback = ximdestroy};
    XICCallback icdestroy = {.client_data = NULL, .callback = xicdestroy};

    xw.ime.xim = XOpenIM(xw.dpy, NULL, NULL, NULL);
    if (xw.ime.xim == NULL)
        return 0;

    if (XSetIMValues(xw.ime.xim, XNDestroyCallback, &imdestroy, NULL))
        fprintf(
            stderr, "XSetIMValues: "
                    "Could not set XNDestroyCallback.\n"
        );

    xw.ime.spotlist = XVaCreateNestedList(0, XNSpotLocation, &xw.ime.spot, NULL);

    if (xw.ime.xic == NULL)
    {
        xw.ime.xic = XCreateIC(xw.ime.xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, xw.win, XNDestroyCallback, &icdestroy, NULL);
    }
    if (xw.ime.xic == NULL)
        fprintf(stderr, "XCreateIC: Could not create input context.\n");

    return 1;
}

void ximinstantiate(Display* dpy, XPointer client, XPointer call)
{
    if (ximopen(dpy))
        XUnregisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL, ximinstantiate, NULL);
}

void ximdestroy(XIM xim, XPointer client, XPointer call)
{
    xw.ime.xim = NULL;
    XRegisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL, ximinstantiate, NULL);
    XFree(xw.ime.spotlist);
}

int xicdestroy(XIC xim, XPointer client, XPointer call)
{
    xw.ime.xic = NULL;
    return 1;
}

void xinit(int cols, int rows)
{
    XGCValues         gcvalues;
    Window            parent;
    pid_t             thispid = getpid();
    XWindowAttributes attr;
    XVisualInfo       vis;

    if (!(xw.dpy = XOpenDisplay(NULL)))
        die("can't open display\n");
    xw.scr = XDefaultScreen(xw.dpy);

    if (!(opt_embed && (parent = strtol(opt_embed, NULL, 0))))
    {
        parent   = XRootWindow(xw.dpy, xw.scr);
        xw.depth = 32;
    }
    else
    {
        XGetWindowAttributes(xw.dpy, parent, &attr);
        xw.depth = attr.depth;
    }

    XMatchVisualInfo(xw.dpy, xw.scr, xw.depth, TrueColor, &vis);
    xw.vis = vis.visual;

    /* font */
    if (!FcInit())
        die("could not init fontconfig.\n");

    usedfont = (opt_font == NULL) ? font : opt_font;
    xloadfonts((char*)usedfont, 0);

    /* colors */
    xw.cmap = XCreateColormap(xw.dpy, parent, xw.vis, 0);
    xloadcols();

    /* adjust fixed window geometry */
    win.w = 2 * win.hborderpx + 2 * borderpx + cols * win.cw;
    win.h = 2 * win.vborderpx + 2 * borderpx + rows * win.ch;
    if (xw.gm & XNegative)
        xw.l += DisplayWidth(xw.dpy, xw.scr) - win.w - 2;
    if (xw.gm & YNegative)
        xw.t += DisplayHeight(xw.dpy, xw.scr) - win.h - 2;

    /* Events */
    xw.attrs.background_pixel = dc.col[defaultbg].pixel;
    xw.attrs.border_pixel     = dc.col[defaultbg].pixel;
    xw.attrs.bit_gravity      = NorthWestGravity;
    xw.attrs.event_mask =
        FocusChangeMask | KeyPressMask | KeyReleaseMask | ExposureMask | VisibilityChangeMask | StructureNotifyMask | ButtonMotionMask | ButtonPressMask | ButtonReleaseMask;
    xw.attrs.colormap = xw.cmap;

    xw.win =
        XCreateWindow(xw.dpy, parent, xw.l, xw.t, win.w, win.h, 0, xw.depth, InputOutput, xw.vis, CWBackPixel | CWBorderPixel | CWBitGravity | CWEventMask | CWColormap, &xw.attrs);

    memset(&gcvalues, 0, sizeof(gcvalues));
    gcvalues.graphics_exposures = False;
    xw.buf                      = XCreatePixmap(xw.dpy, xw.win, win.w, win.h, xw.depth);
    dc.gc                       = XCreateGC(xw.dpy, xw.buf, GCGraphicsExposures, &gcvalues);
    XSetForeground(xw.dpy, dc.gc, dc.col[defaultbg].pixel);
    XFillRectangle(xw.dpy, xw.buf, dc.gc, 0, 0, win.w, win.h);

    /* font spec buffer */
    xw.specbuf = xmalloc<GlyphFontSpec>(cols * sizeof(GlyphFontSpec));

    /* Xft rendering context */
    xw.draw = XftDrawCreate(xw.dpy, xw.buf, xw.vis, xw.cmap);

    /* input methods */
    if (!ximopen(xw.dpy))
        XRegisterIMInstantiateCallback(xw.dpy, NULL, NULL, NULL, ximinstantiate, NULL);

    /* white cursor, black outline */
    cursor = XcursorLibraryLoadCursor(xw.dpy, mouseshape);
    XDefineCursor(xw.dpy, xw.win, cursor);

    xw.xembed      = XInternAtom(xw.dpy, "_XEMBED", False);
    xw.wmdeletewin = XInternAtom(xw.dpy, "WM_DELETE_WINDOW", False);
    xw.netwmname   = XInternAtom(xw.dpy, "_NET_WM_NAME", False);
    XSetWMProtocols(xw.dpy, xw.win, &xw.wmdeletewin, 1);

    xw.netwmpid = XInternAtom(xw.dpy, "_NET_WM_PID", False);
    XChangeProperty(xw.dpy, xw.win, xw.netwmpid, XA_CARDINAL, 32, PropModeReplace, (uchar*)&thispid, 1);

    win.mode = MODE_NUMLOCK;
    resettitle();
    xhints();
    XMapWindow(xw.dpy, xw.win);

    std::array<uint32_t, 4> data{0,0,0,0};
    xw.blur       = XInternAtom(xw.dpy, "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
    XChangeProperty(xw.dpy, xw.win, xw.blur, XA_CARDINAL, 32, PropModeReplace, (_Xconst unsigned char*)data.data(), 4);

    XSync(xw.dpy, False);

    clock_gettime(CLOCK_MONOTONIC, &xsel.tclick1);
    clock_gettime(CLOCK_MONOTONIC, &xsel.tclick2);
    xsel.primary   = NULL;
    xsel.clipboard = NULL;
    xsel.xtarget   = XInternAtom(xw.dpy, "UTF8_STRING", 0);
    if (xsel.xtarget == 0)
        xsel.xtarget = XA_STRING;

    boxdraw_xinit(xw.dpy, xw.cmap, xw.draw, xw.vis);
}

int xmakeglyphfontspecs(XftGlyphFontSpec* specs, Glyph const* glyphs, int len, int x, int y)
{
    float      winx = win.hborderpx + x * win.cw, winy = win.vborderpx + y * win.ch, xp, yp;
    ushort     mode, prevmode                          = USHRT_MAX;
    Font*      font      = &dc.font;
    int        frcflags  = FRC_NORMAL;
    float      runewidth = win.cw;
    Rune       rune;
    FT_UInt    glyphidx;
    FcResult   fcres;
    FcPattern* fcpattern,* fontpattern;
    FcFontSet* fcsets[] = {NULL};
    FcCharSet* fccharset;
    int        i, f, numspecs = 0;

    for (i = 0, xp = winx, yp = winy + font->ascent; i < len; ++i)
    {
        /* Fetch rune and mode for current glyph. */
        rune = glyphs[i].u;
        mode = glyphs[i].mode;

        /* Skip dummy wide-character spacing. */
        if (mode & ATTR_WDUMMY)
            continue;

        /* Determine font for glyph if different from previous glyph. */
        if (prevmode != mode)
        {
            prevmode  = mode;
            font      = &dc.font;
            frcflags  = FRC_NORMAL;
            runewidth = win.cw * ((mode & ATTR_WIDE) ? 2.0f : 1.0f);
            if ((mode & ATTR_ITALIC) && (mode & ATTR_BOLD))
            {
                font     = &dc.ibfont;
                frcflags = FRC_ITALICBOLD;
            }
            else if (mode & ATTR_ITALIC)
            {
                font     = &dc.ifont;
                frcflags = FRC_ITALIC;
            }
            else if (mode & ATTR_BOLD)
            {
                font     = &dc.bfont;
                frcflags = FRC_BOLD;
            }
            yp = winy + font->ascent;
        }

        if (mode & ATTR_BOXDRAW)
        {
            /* minor shoehorning: boxdraw uses only this ushort */
            glyphidx = boxdrawindex(&glyphs[i]);
        }
        else
        {
            /* Lookup character index with default font. */
            glyphidx = XftCharIndex(xw.dpy, font->match, rune);
        }
        if (glyphidx)
        {
            specs[numspecs].font  = font->match;
            specs[numspecs].glyph = glyphidx;
            specs[numspecs].x     = (short)xp;
            specs[numspecs].y     = (short)yp;
            xp += runewidth;
            numspecs++;
            continue;
        }

        /* Fallback on font cache, search the font cache for match. */
        for (f = 0; f < frclen; f++)
        {
            glyphidx = XftCharIndex(xw.dpy, frc[f].font, rune);
            /* Everything correct. */
            if (glyphidx && frc[f].flags == frcflags)
                break;
            /* We got a default font for a not found glyph. */
            if (!glyphidx && frc[f].flags == frcflags && frc[f].unicodep == rune)
            {
                break;
            }
        }

        /* Nothing was found. Use fontconfig to find matching font. */
        if (f >= frclen)
        {
            if (!font->set)
                font->set = FcFontSort(0, font->pattern, 1, 0, &fcres);
            fcsets[0] = font->set;

            /*
             * Nothing was found in the cache. Now use
             * some dozen of Fontconfig calls to get the
             * font for one single character.
             *
             * Xft and fontconfig are design failures.
             */
            fcpattern = FcPatternDuplicate(font->pattern);
            fccharset = FcCharSetCreate();

            FcCharSetAddChar(fccharset, rune);
            FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
            FcPatternAddBool(fcpattern, FC_SCALABLE, 1);

            FcConfigSubstitute(0, fcpattern, FcMatchPattern);
            FcDefaultSubstitute(fcpattern);

            fontpattern = FcFontSetMatch(0, fcsets, 1, fcpattern, &fcres);

            /* Allocate memory for the new cache entry. */
            if (frclen >= frccap)
            {
                frccap += 16;
                frc = (Fontcache*)xrealloc(frc, frccap * sizeof(Fontcache));
            }

            frc[frclen].font = XftFontOpenPattern(xw.dpy, fontpattern);
            if (!frc[frclen].font)
                die("XftFontOpenPattern failed seeking fallback font: %s\n", strerror(errno));
            frc[frclen].flags    = frcflags;
            frc[frclen].unicodep = rune;

            glyphidx = XftCharIndex(xw.dpy, frc[frclen].font, rune);

            f = frclen;
            frclen++;

            FcPatternDestroy(fcpattern);
            FcCharSetDestroy(fccharset);
        }

        specs[numspecs].font  = frc[f].font;
        specs[numspecs].glyph = glyphidx;
        specs[numspecs].x     = (short)xp;
        specs[numspecs].y     = (short)yp;
        xp += runewidth;
        numspecs++;
    }

    /* Harfbuzz transformation for ligatures. */
    hbtransform(specs, glyphs, len, x, y);

    return numspecs;
}

static int isSlopeRising(int x, int iPoint, int waveWidth)
{
    //    .     .     .     .
    //   / \   / \   / \   / \
	//  /   \ /   \ /   \ /   \
	// .     .     .     .     .

    // Find absolute `x` of point
    x += iPoint * (waveWidth / 2);

    // Find index of absolute wave
    int absSlope = x / ((float)waveWidth / 2);

    return (absSlope % 2);
}

static int getSlope(int x, int iPoint, int waveWidth)
{
    // Sizes: Caps are half width of slopes
    //    1_2       1_2       1_2      1_2
    //   /   \     /   \     /   \    /   \
	//  /     \   /     \   /     \  /     \
	// 0       3_0       3_0      3_0       3_
    // <2->    <1>         <---6---->

    // Find type of first point
    int firstType;
    x -= (x / waveWidth) * waveWidth;
    if (x < (waveWidth * (2.f / 6.f)))
        firstType = UNDERCURL_SLOPE_ASCENDING;
    else if (x < (waveWidth * (3.f / 6.f)))
        firstType = UNDERCURL_SLOPE_TOP_CAP;
    else if (x < (waveWidth * (5.f / 6.f)))
        firstType = UNDERCURL_SLOPE_DESCENDING;
    else
        firstType = UNDERCURL_SLOPE_BOTTOM_CAP;

    // Find type of given point
    int pointType = (iPoint % 4);
    pointType += firstType;
    pointType %= 4;

    return pointType;
}

void xdrawglyphfontspecs(XftGlyphFontSpec const* specs, Glyph base, int len, int x, int y)
{
    int          charlen = len * ((base.mode & ATTR_WIDE) ? 2 : 1);
    int          winx = win.hborderpx + x * win.cw, winy = win.vborderpx + y * win.ch, width = charlen * win.cw;
    Color *      fg, *bg, *temp, revfg, revbg, truefg, truebg;
    XRenderColor colfg, colbg;
    XRectangle   r;

    /* Fallback on color display for attributes not supported by the font */
    if (base.mode & ATTR_ITALIC && base.mode & ATTR_BOLD)
    {
        if (dc.ibfont.badslant || dc.ibfont.badweight)
            base.fg = defaultattr;
    }
    else if ((base.mode & ATTR_ITALIC && dc.ifont.badslant) || (base.mode & ATTR_BOLD && dc.bfont.badweight))
    {
        base.fg = defaultattr;
    }

    if (IS_TRUECOL(base.fg))
    {
        colfg.alpha = 0xffff;
        colfg.red   = TRUERED(base.fg);
        colfg.green = TRUEGREEN(base.fg);
        colfg.blue  = TRUEBLUE(base.fg);
        XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &truefg);
        fg = &truefg;
    }
    else
    {
        fg = &dc.col[base.fg];
    }

    if (IS_TRUECOL(base.bg))
    {
        colbg.alpha = 0xffff;
        colbg.green = TRUEGREEN(base.bg);
        colbg.red   = TRUERED(base.bg);
        colbg.blue  = TRUEBLUE(base.bg);
        XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg, &truebg);
        bg = &truebg;
    }
    else
    {
        bg = &dc.col[base.bg];
    }

    /* Change basic system colors [0-7] to bright system colors [8-15] */
    if ((base.mode & ATTR_BOLD_FAINT) == ATTR_BOLD && BETWEEN(base.fg, 0, 7))
        fg = &dc.col[base.fg + 8];

    if (IS_SET(MODE_REVERSE))
    {
        if (fg == &dc.col[defaultfg])
        {
            fg = &dc.col[defaultbg];
        }
        else
        {
            colfg.red   = ~fg->color.red;
            colfg.green = ~fg->color.green;
            colfg.blue  = ~fg->color.blue;
            colfg.alpha = fg->color.alpha;
            XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &revfg);
            fg = &revfg;
        }

        if (bg == &dc.col[defaultbg])
        {
            bg = &dc.col[defaultfg];
        }
        else
        {
            colbg.red   = ~bg->color.red;
            colbg.green = ~bg->color.green;
            colbg.blue  = ~bg->color.blue;
            colbg.alpha = bg->color.alpha;
            XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg, &revbg);
            bg = &revbg;
        }
    }

    if ((base.mode & ATTR_BOLD_FAINT) == ATTR_FAINT)
    {
        colfg.red   = fg->color.red / 2;
        colfg.green = fg->color.green / 2;
        colfg.blue  = fg->color.blue / 2;
        colfg.alpha = fg->color.alpha;
        XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &revfg);
        fg = &revfg;
    }

    if (base.mode & ATTR_REVERSE)
    {
        temp = fg;
        fg   = bg;
        bg   = temp;
    }

    if (base.mode & ATTR_BLINK && win.mode & MODE_BLINK)
        fg = bg;

    if (base.mode & ATTR_INVISIBLE)
        fg = bg;

    /* Intelligent cleaning up of the borders. */
    if (x == 0)
    {
        xclear(0, (y == 0) ? 0 : winy, win.vborderpx, winy + win.ch + ((winy + win.ch >= win.vborderpx + win.th) ? win.h : 0));
    }
    if (winx + width >= win.hborderpx + win.tw)
    {
        xclear(winx + width, (y == 0) ? 0 : winy, win.w, ((winy + win.ch >= win.vborderpx + win.th) ? win.h : (winy + win.ch)));
    }
    if (y == 0)
        xclear(winx, 0, winx + width, win.vborderpx);
    if (winy + win.ch >= win.vborderpx + win.th)
        xclear(winx, winy + win.ch, winx + width, win.h);

    /* Clean up the region we want to draw to. */
    XftDrawRect(xw.draw, bg, winx, winy, width, win.ch);

    /* Set the clip region because Xft is sometimes dirty. */
    r.x      = 0;
    r.y      = 0;
    r.height = win.ch;
    r.width  = width;
    XftDrawSetClipRectangles(xw.draw, winx, winy, &r, 1);

    if (base.mode & ATTR_BOXDRAW)
    {
        drawboxes(winx, winy, width / len, win.ch, fg, bg, specs, len);
    }
    else
    {
        /* Render the glyphs. */
        XftDrawGlyphFontSpec(xw.draw, fg, specs, len);
    }

    /* Render underline and strikethrough. */
    if (base.mode & ATTR_UNDERLINE)
    {
        // Underline Color
        int const widthThreshold = 28;                            // +1 width every widthThreshold px of font
        int       wlw            = (win.ch / widthThreshold) + 1; // Wave Line Width
        int       linecolor;
        if ((base.ucolor[0] >= 0) && !(base.mode & ATTR_BLINK && win.mode & MODE_BLINK) && !(base.mode & ATTR_INVISIBLE))
        {
            // Special color for underline
            // Index
            if (base.ucolor[1] < 0)
            {
                linecolor = dc.col[base.ucolor[0]].pixel;
            }
            // RGB
            else
            {
                XColor lcolor;
                lcolor.red   = base.ucolor[0] * 257;
                lcolor.green = base.ucolor[1] * 257;
                lcolor.blue  = base.ucolor[2] * 257;
                lcolor.flags = DoRed | DoGreen | DoBlue;
                XAllocColor(xw.dpy, xw.cmap, &lcolor);
                linecolor = lcolor.pixel;
            }
        }
        else
        {
            // Foreground color for underline
            linecolor = fg->pixel;
        }

        XGCValues ugcv = {.foreground = (long unsigned int)linecolor, .line_width = wlw, .line_style = LineSolid, .cap_style = CapNotLast};

        GC ugc = XCreateGC(xw.dpy, XftDrawDrawable(xw.draw), GCForeground | GCLineWidth | GCLineStyle | GCCapStyle, &ugcv);

        // Underline Style
        if (base.ustyle != 3)
        {
            // XftDrawRect(xw.draw, fg, winx, winy + dc.font.ascent + 1, width, 1);
            XFillRectangle(xw.dpy, XftDrawDrawable(xw.draw), ugc, winx, winy + dc.font.ascent + 1, width, wlw);
        }
        else if (base.ustyle == 3)
        {
            int ww = win.cw;                        // width;
            int wh = dc.font.descent - wlw / 2 - 1; // r.height/7;
            int wx = winx;
            int wy = winy + win.ch - dc.font.descent;

#if UNDERCURL_STYLE == UNDERCURL_CURLY
            // Draw waves
            int   narcs = charlen * 2 + 1;
            XArc* arcs  = xmalloc(sizeof(XArc) * narcs);

            int i = 0;
            for (i = 0; i < charlen - 1; i++)
            {
                arcs[i * 2]     = (XArc){.x = wx + win.cw * i + ww / 4, .y = wy, .width = win.cw / 2, .height = wh, .angle1 = 0, .angle2 = 180 * 64};
                arcs[i * 2 + 1] = (XArc){.x = wx + win.cw * i + ww * 0.75, .y = wy, .width = win.cw / 2, .height = wh, .angle1 = 180 * 64, .angle2 = 180 * 64};
            }
            // Last wave
            arcs[i * 2] = (XArc){wx + ww * i + ww / 4, wy, ww / 2, wh, 0, 180 * 64};
            // Last wave tail
            arcs[i * 2 + 1] = (XArc){wx + ww * i + ww * 0.75, wy, ceil(ww / 2.), wh, 180 * 64, 90 * 64};
            // First wave tail
            i++;
            arcs[i * 2] = (XArc){wx - ww / 4 - 1, wy, ceil(ww / 2.), wh, 270 * 64, 90 * 64};

            XDrawArcs(xw.dpy, XftDrawDrawable(xw.draw), ugc, arcs, narcs);

            free(arcs);
#elif UNDERCURL_STYLE == UNDERCURL_SPIKY
            // Make the underline corridor larger
            /*
            wy -= wh;
            */
            wh *= 2;

            // Set the angle of the slope to 45??
            ww = wh;

            // Position of wave is independent of word, it's absolute
            wx = (wx / (ww / 2)) * (ww / 2);

            int marginStart = winx - wx;

            // Calculate number of points with floating precision
            float n = width;        // Width of word in pixels
            n       = (n / ww) * 2; // Number of slopes (/ or \)
            n += 2;                 // Add two last points
            int npoints = n;        // Convert to int

            // Total length of underline
            float waveLength = 0;

            if (npoints >= 3)
            {
                // We add an aditional slot in case we use a bonus point
                auto points = xmalloc<XPoint>(sizeof(XPoint) * (npoints + 1));

                // First point (Starts with the word bounds)
                points[0] = XPoint{.x = short(wx + marginStart), .y = short((isSlopeRising(wx, 0, ww)) ? (wy - marginStart + ww / 2.f) : (wy + marginStart))};

                // Second point (Goes back to the absolute point coordinates)
                points[1] = XPoint{.x = short((ww / 2.f) - marginStart), .y = short((isSlopeRising(wx, 1, ww)) ? (ww / 2.f - marginStart) : (-ww / 2.f + marginStart))};
                waveLength += (ww / 2.f) - marginStart;

                // The rest of the points
                for (int i = 2; i < npoints - 1; i++)
                {
                    points[i] = XPoint{.x = short(ww / 2), .y = short((isSlopeRising(wx, i, ww)) ? wh / 2 : -wh / 2)};
                    waveLength += ww / 2;
                }

                // Last point
                points[npoints - 1] = XPoint{.x = short(ww / 2), .y = short((isSlopeRising(wx, npoints - 1, ww)) ? wh / 2 : -wh / 2)};
                waveLength += ww / 2;

                // End
                if (waveLength < width)
                { // Add a bonus point?
                    int marginEnd   = width - waveLength;
                    points[npoints] = XPoint{.x = short(marginEnd), .y = short((isSlopeRising(wx, npoints, ww)) ? (marginEnd) : (-marginEnd))};

                    npoints++;
                }
                else if (waveLength > width)
                { // Is last point too far?
                    int marginEnd = waveLength - width;
                    points[npoints - 1].x -= marginEnd;
                    if (isSlopeRising(wx, npoints - 1, ww))
                        points[npoints - 1].y -= (marginEnd);
                    else
                        points[npoints - 1].y += (marginEnd);
                }

                // Draw the lines
                XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), ugc, points, npoints, CoordModePrevious);

                // Draw a second underline with an offset of 1 pixel
                if (((win.ch / (widthThreshold / 2)) % 2))
                {
                    points[0].x++;

                    XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), ugc, points, npoints, CoordModePrevious);
                }

                // Free resources
                free(points);
            }
#else // UNDERCURL_CAPPED
      // Cap is half of wave width
            float capRatio = 0.5f;

            // Make the underline corridor larger
            wh *= 2;

            // Set the angle of the slope to 45??
            ww = wh;
            ww *= 1 + capRatio; // Add a bit of width for the cap

            // Position of wave is independent of word, it's absolute
            wx = (wx / ww) * ww;

            float marginStart;
            switch (getSlope(winx, 0, ww))
            {
            case UNDERCURL_SLOPE_ASCENDING:
                marginStart = winx - wx;
                break;
            case UNDERCURL_SLOPE_TOP_CAP:
                marginStart = winx - (wx + (ww * (2.f / 6.f)));
                break;
            case UNDERCURL_SLOPE_DESCENDING:
                marginStart = winx - (wx + (ww * (3.f / 6.f)));
                break;
            case UNDERCURL_SLOPE_BOTTOM_CAP:
                marginStart = winx - (wx + (ww * (5.f / 6.f)));
                break;
            }

            // Calculate number of points with floating precision
            float n = width;  // Width of word in pixels
                              //					   ._.
            n = (n / ww) * 4; // Number of points (./   \.)
            n += 2;           // Add two last points
            int npoints = n;  // Convert to int

            // Position of the pen to draw the lines
            float penX = 0;
            float penY = 0;

            if (npoints >= 3)
            {
                XPoint* points = xmalloc(sizeof(XPoint) * (npoints + 1));

                // First point (Starts with the word bounds)
                penX = winx;
                switch (getSlope(winx, 0, ww))
                {
                case UNDERCURL_SLOPE_ASCENDING:
                    penY = wy + wh / 2.f - marginStart;
                    break;
                case UNDERCURL_SLOPE_TOP_CAP:
                    penY = wy;
                    break;
                case UNDERCURL_SLOPE_DESCENDING:
                    penY = wy + marginStart;
                    break;
                case UNDERCURL_SLOPE_BOTTOM_CAP:
                    penY = wy + wh / 2.f;
                    break;
                }
                points[0].x = penX;
                points[0].y = penY;

                // Second point (Goes back to the absolute point coordinates)
                switch (getSlope(winx, 1, ww))
                {
                case UNDERCURL_SLOPE_ASCENDING:
                    penX += ww * (1.f / 6.f) - marginStart;
                    penY += 0;
                    break;
                case UNDERCURL_SLOPE_TOP_CAP:
                    penX += ww * (2.f / 6.f) - marginStart;
                    penY += -wh / 2.f + marginStart;
                    break;
                case UNDERCURL_SLOPE_DESCENDING:
                    penX += ww * (1.f / 6.f) - marginStart;
                    penY += 0;
                    break;
                case UNDERCURL_SLOPE_BOTTOM_CAP:
                    penX += ww * (2.f / 6.f) - marginStart;
                    penY += -marginStart + wh / 2.f;
                    break;
                }
                points[1].x = penX;
                points[1].y = penY;

                // The rest of the points
                for (int i = 2; i < npoints; i++)
                {
                    switch (getSlope(winx, i, ww))
                    {
                    case UNDERCURL_SLOPE_ASCENDING:
                    case UNDERCURL_SLOPE_DESCENDING:
                        penX += ww * (1.f / 6.f);
                        penY += 0;
                        break;
                    case UNDERCURL_SLOPE_TOP_CAP:
                        penX += ww * (2.f / 6.f);
                        penY += -wh / 2.f;
                        break;
                    case UNDERCURL_SLOPE_BOTTOM_CAP:
                        penX += ww * (2.f / 6.f);
                        penY += wh / 2.f;
                        break;
                    }
                    points[i].x = penX;
                    points[i].y = penY;
                }

                // End
                float waveLength = penX - winx;
                if (waveLength < width)
                { // Add a bonus point?
                    int marginEnd = width - waveLength;
                    penX += marginEnd;
                    switch (getSlope(winx, npoints, ww))
                    {
                    case UNDERCURL_SLOPE_ASCENDING:
                    case UNDERCURL_SLOPE_DESCENDING:
                        // penY += 0;
                        break;
                    case UNDERCURL_SLOPE_TOP_CAP:
                        penY += -marginEnd;
                        break;
                    case UNDERCURL_SLOPE_BOTTOM_CAP:
                        penY += marginEnd;
                        break;
                    }

                    points[npoints].x = penX;
                    points[npoints].y = penY;

                    npoints++;
                }
                else if (waveLength > width)
                { // Is last point too far?
                    int marginEnd = waveLength - width;
                    points[npoints - 1].x -= marginEnd;
                    switch (getSlope(winx, npoints - 1, ww))
                    {
                    case UNDERCURL_SLOPE_TOP_CAP:
                        points[npoints - 1].y += marginEnd;
                        break;
                    case UNDERCURL_SLOPE_BOTTOM_CAP:
                        points[npoints - 1].y -= marginEnd;
                        break;
                    default:
                        break;
                    }
                }

                // Draw the lines
                XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), ugc, points, npoints, CoordModeOrigin);

                // Draw a second underline with an offset of 1 pixel
                if (((win.ch / (widthThreshold / 2)) % 2))
                {
                    for (int i = 0; i < npoints; i++)
                        points[i].x++;

                    XDrawLines(xw.dpy, XftDrawDrawable(xw.draw), ugc, points, npoints, CoordModeOrigin);
                }

                // Free resources
                free(points);
            }
#endif
        }

        XFreeGC(xw.dpy, ugc);
    }

    if (base.mode & ATTR_STRUCK)
    {
        XftDrawRect(xw.draw, fg, winx, winy + 2 * dc.font.ascent / 3, width, 1);
    }

    /* Reset clip to none. */
    XftDrawSetClip(xw.draw, 0);
}

void xdrawglyph(Glyph g, int x, int y)
{
    int              numspecs;
    XftGlyphFontSpec spec;

    numspecs = xmakeglyphfontspecs(&spec, &g, 1, x, y);
    xdrawglyphfontspecs(&spec, g, numspecs, x, y);
}

void xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og, Line line, int len)
{
    Color        drawcol;
    XRenderColor colbg;

    /* remove the old cursor */
    if (con.selected(ox, oy))
        og.mode ^= ATTR_REVERSE;

    /* Redraw the line where cursor was previously.
     * It will restore the ligatures broken by the cursor. */
    xdrawline(line, 0, oy, len);

    if (IS_SET(MODE_HIDE))
        return;

    /*
     * Select the right color for the right mode.
     */
    g.mode &= ATTR_BOLD | ATTR_ITALIC | ATTR_UNDERLINE | ATTR_STRUCK | ATTR_WIDE | ATTR_BOXDRAW;

    if (IS_SET(MODE_REVERSE))
    {
        g.mode |= ATTR_REVERSE;
        g.bg = defaultfg;
        if (con.selected(cx, cy))
        {
            drawcol = dc.col[defaultcs];
            g.fg    = defaultrcs;
        }
        else
        {
            drawcol = dc.col[defaultrcs];
            g.fg    = defaultcs;
        }
    }
    else
    {
        if (con.selected(cx, cy))
        {
            g.fg = defaultfg;
            g.bg = defaultrcs;
        }
        else
        {
            /** this is the main part of the dynamic cursor color patch */
            g.bg = g.fg;
            g.fg = defaultbg;
        }
        /**
         * and this is the second part of the dynamic cursor color patch.
         * it handles the `drawcol` variable
         */
        if (IS_TRUECOL(g.bg))
        {
            colbg.alpha = 0xffff;
            colbg.red   = TRUERED(g.bg);
            colbg.green = TRUEGREEN(g.bg);
            colbg.blue  = TRUEBLUE(g.bg);
            XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg, &drawcol);
        }
        else
        {
            drawcol = dc.col[g.bg];
        }
    }

    /* draw the new one */
    if (IS_SET(MODE_FOCUSED))
    {
        switch (win.cursor)
        {
        case 7:           /* st extension */
            g.u = 0x2603; /* snowman (U+2603) */
                          /* FALLTHROUGH */
        case 0:           /* Blinking Block */
        case 1:           /* Blinking Block (Default) */
        case 2:           /* Steady Block */
            xdrawglyph(g, cx, cy);
            break;
        case 3: /* Blinking Underline */
        case 4: /* Steady Underline */
            XftDrawRect(xw.draw, &drawcol, win.hborderpx + cx * win.cw, win.vborderpx + (cy + 1) * win.ch - cursorthickness, win.cw, cursorthickness);
            break;
        case 5: /* Blinking bar */
        case 6: /* Steady bar */
            XftDrawRect(xw.draw, &drawcol, win.hborderpx + cx * win.cw, win.vborderpx + cy * win.ch, cursorthickness, win.ch);
            break;
        }
    }
    else
    {
        XftDrawRect(xw.draw, &drawcol, win.hborderpx + cx * win.cw, win.vborderpx + cy * win.ch, win.cw - 1, 1);
        XftDrawRect(xw.draw, &drawcol, win.hborderpx + cx * win.cw, win.vborderpx + cy * win.ch, 1, win.ch - 1);
        XftDrawRect(xw.draw, &drawcol, win.hborderpx + (cx + 1) * win.cw - 1, win.vborderpx + cy * win.ch, 1, win.ch - 1);
        XftDrawRect(xw.draw, &drawcol, win.hborderpx + cx * win.cw, win.vborderpx + (cy + 1) * win.ch - 1, win.cw, 1);
    }
}

void xsetenv(void)
{
    char buf[sizeof(long) * 8 + 1];

    snprintf(buf, sizeof(buf), "%lu", xw.win);
    setenv("WINDOWID", buf, 1);
}

void xfreetitlestack(void)
{
    for (int i = 0; i < LEN(titlestack); i++)
    {
        free(titlestack[i]);
        titlestack[i] = NULL;
    }
}

void xsettitle(char* p, int pop)
{
    XTextProperty prop;

    free(titlestack[tstki]);
    if (pop)
    {
        titlestack[tstki] = NULL;
        tstki             = (tstki - 1 + TITLESTACKSIZE) % TITLESTACKSIZE;
        p                 = titlestack[tstki] ? titlestack[tstki] : opt_title;
    }
    else if (p)
    {
        titlestack[tstki] = xstrdup(p);
    }
    else
    {
        titlestack[tstki] = NULL;
        p                 = opt_title;
    }

    Xutf8TextListToTextProperty(xw.dpy, &p, 1, XUTF8StringStyle, &prop);
    XSetWMName(xw.dpy, xw.win, &prop);
    XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmname);
    XFree(prop.value);
}

void xpushtitle(void)
{
    int tstkin = (tstki + 1) % TITLESTACKSIZE;

    free(titlestack[tstkin]);
    titlestack[tstkin] = titlestack[tstki] ? xstrdup(titlestack[tstki]) : NULL;
    tstki              = tstkin;
}

int xstartdraw(void)
{
    if (IS_SET(MODE_VISIBLE))
        XCopyArea(xw.dpy, xw.win, xw.buf, dc.gc, 0, 0, win.w, win.h, 0, 0);
    return IS_SET(MODE_VISIBLE);
}

void xdrawsixel(size_t col, size_t row)
{
    int n = 0;
    int nlimit = 256;

    for (auto& image : con.term.images)
    {
        if (image.drawable == nullptr)
        {
            auto depth    = DefaultDepth(xw.dpy, xw.scr);
            auto drawable = XCreatePixmap(xw.dpy, xw.win, image.width, image.height, depth);

            XImage ximage           = {0};
            ximage.format           = ZPixmap,
            ximage.data             = (char*)image.data.data();
            ximage.width            = image.width,
            ximage.height           = image.height,
            ximage.xoffset          = 0,
            ximage.byte_order       = LSBFirst,
            ximage.bitmap_bit_order = MSBFirst,
            ximage.bits_per_pixel   = 32,
            ximage.bytes_per_line   = image.width * 4,
            ximage.bitmap_unit      = 32,
            ximage.bitmap_pad       = 32,
            ximage.depth            = depth;

            XGCValues gcvalues = {0};
            auto      gc       = XCreateGC(xw.dpy, drawable, 0, &gcvalues);
            XPutImage(xw.dpy, drawable, gc, &ximage, 0, 0, 0, 0, image.width, image.height);
            XFlush(xw.dpy);
            XFreeGC(xw.dpy, gc);
            image.drawable = (void*)drawable;
        }

        std::vector<XRectangle> rects;

        for (auto y = image.y; y < (image.y + (image.height + win.ch - 1) / win.ch) && y < row; y++)
        {
            if (y >= 0)
            {
                for (auto x = image.x; x < (image.x + (image.width + win.cw - 1) / win.cw) && x < col; x++)
                {
                    // if (line[y][x].mode & ATTR_SIXEL)
                    {
                        if (!rects.empty())
                        {
                            if (rects[rects.size() - 1].x + rects[rects.size() - 1].width == borderpx + x * win.cw && rects[rects.size() - 1].y == borderpx + y * win.ch)
                            {
                                rects[rects.size() - 1].width += win.cw;
                            }
                            else
                            {
                                XRectangle rect;
                                rect.x = borderpx + x * win.cw;
                                rect.y = borderpx + y * win.ch;
                                rect.width = win.cw;
                                rect.height = win.ch;
                                rects.push_back(rect);
                            }
                        }
                        else
                        {
                            XRectangle rect;
                            rect.x = borderpx + x * win.cw;
                            rect.y = borderpx + y * win.ch;
                            rect.width = win.cw;
                            rect.height = win.ch;
                            rects.push_back(rect);
                        }
                    }
                }
            }
            if (rects.size() > 2 && rects[rects.size() - 2].x == rects[rects.size() - 1].x && rects[rects.size() - 2].width == rects[rects.size() - 1].width)
            {
                if (rects[rects.size() - 2].y + rects[rects.size() - 2].height == rects[rects.size() - 1].y)
                {
                    rects[rects.size() - 2].height += win.ch;
                    n--;
                }
            }
        }

        if (con.term.top <= image.y && image.y < con.term.bot)
        {
            XGCValues gcvalues = {0};
            auto      gc       = XCreateGC(xw.dpy, xw.win, 0, &gcvalues);
            if (!rects.empty())
                XSetClipRectangles(xw.dpy, gc, 0, 0, rects.data(), rects.size(), YXSorted);

            auto v = win.hborderpx + image.x * win.cw;
            auto h = win.vborderpx + image.y * win.ch;
            XCopyArea(xw.dpy, (Drawable)image.drawable, xw.buf, gc, 0, 0, image.width, image.height, v, h);
            XFreeGC(xw.dpy, gc);
        }
    }
}

void xdrawline(Line line, int x1, int y1, int x2)
{
    int               i, x, ox, numspecs;
    Glyph             base;
    XftGlyphFontSpec* specs = xw.specbuf;

    numspecs = xmakeglyphfontspecs(specs, &line[x1], x2 - x1, x1, y1);
    i = ox = 0;
    for (x = x1; x < x2 && i < numspecs; x++)
    {
        auto& new_ = line[x];
        if (new_.mode == ATTR_WDUMMY)
            continue;

        if (con.selected(x, y1))
            new_.mode ^= ATTR_REVERSE;
        if (i > 0 && ATTRCMP(base, new_))
        {
            xdrawglyphfontspecs(specs, base, i, ox, y1);
            specs += i;
            numspecs -= i;
            i = 0;
        }
        if (i == 0)
        {
            ox   = x;
            base = new_;
        }
        i++;
    }
    if (i > 0)
        xdrawglyphfontspecs(specs, base, i, ox, y1);
}

void xfinishdraw(void)
{
    XCopyArea(xw.dpy, xw.buf, xw.win, dc.gc, 0, 0, win.w, win.h, 0, 0);
    XSetForeground(xw.dpy, dc.gc, dc.col[IS_SET(MODE_REVERSE) ? defaultfg : defaultbg].pixel);
}

void xximspot(int x, int y)
{
    if (xw.ime.xic == NULL)
        return;

    xw.ime.spot.x = borderpx + x * win.cw;
    xw.ime.spot.y = borderpx + (y + 1) * win.ch;

    XSetICValues(xw.ime.xic, XNPreeditAttributes, xw.ime.spotlist, NULL);
}

void expose(XEvent* ev)
{
    redraw();
}

void visibility(XEvent* ev)
{
    XVisibilityEvent* e = &ev->xvisibility;

    MODBIT(win.mode, e->state != VisibilityFullyObscured, MODE_VISIBLE);
}

void unmap(XEvent* ev)
{
    win.mode &= ~MODE_VISIBLE;
}

void xsetpointermotion(int set)
{
    MODBIT(xw.attrs.event_mask, set, PointerMotionMask);
    XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask, &xw.attrs);
}

void xsetmode(int set, unsigned int flags)
{
    int mode = win.mode;
    MODBIT(win.mode, set, flags);
    if (flags & MODE_MOUSE)
    {
        if (win.mode & MODE_MOUSE)
            XUndefineCursor(xw.dpy, xw.win);
        else
            XDefineCursor(xw.dpy, xw.win, cursor);
    }
    if ((win.mode & MODE_REVERSE) != (mode & MODE_REVERSE))
        redraw();
}

int xsetcursor(int cursor)
{
    if (!BETWEEN(cursor, 0, 7)) /* 7: st extension */
        return 1;
    win.cursor = cursor;
    return 0;
}

void xseturgency(int add)
{
    XWMHints* h = XGetWMHints(xw.dpy, xw.win);

    MODBIT(h->flags, add, XUrgencyHint);
    XSetWMHints(xw.dpy, xw.win, h);
    XFree(h);
}

void xbell(void)
{
    if (!(IS_SET(MODE_FOCUSED)))
        xseturgency(1);
    if (bellvolume)
        XkbBell(xw.dpy, xw.win, bellvolume, (Atom)NULL);
}

void focus(XEvent* ev)
{
    XFocusChangeEvent* e = &ev->xfocus;

    if (e->mode == NotifyGrab)
        return;

    if (ev->type == FocusIn)
    {
        if (xw.ime.xic)
            XSetICFocus(xw.ime.xic);
        win.mode |= MODE_FOCUSED;
        xseturgency(0);
        if (IS_SET(MODE_FOCUS))
            con.ttywrite("\033[I", 0);
    }
    else
    {
        if (xw.ime.xic)
            XUnsetICFocus(xw.ime.xic);
        win.mode &= ~MODE_FOCUSED;
        if (IS_SET(MODE_FOCUS))
            con.ttywrite("\033[O", 0);
    }
}

int match(uint mask, uint state)
{
    return mask == XK_ANY_MOD || mask == (state & ~(Mod2Mask|XK_SWITCH_MOD));
}

char const* kmap(KeySym k, uint state)
{
    Key* kp;
    int  i;

    /* Check for mapped keys out of X11 function keys. */
    for (i = 0; i < LEN(mappedkeys); i++)
    {
        if (mappedkeys[i] == k)
            break;
    }
    if (i == LEN(mappedkeys))
    {
        if ((k & 0xFFFF) < 0xFD00)
            return NULL;
    }

    for (kp = key; kp < key + LEN(key); kp++)
    {
        if (kp->k != k)
            continue;

        if (!match(ljh::underlying_cast(kp->mask), state))
            continue;

        if (IS_SET(MODE_APPKEYPAD) ? kp->appkey < indeterminate : kp->appkey > indeterminate)
            continue;
        if (IS_SET(MODE_NUMLOCK) && kp->appkey == double_true)
            continue;

        if (IS_SET(MODE_APPCURSOR) ? kp->appcursor < indeterminate : kp->appcursor > indeterminate)
            continue;

        return kp->s.data();
    }

    return NULL;
}

void kpress(XEvent* ev)
{
    XKeyEvent* e = &ev->xkey;
    KeySym     ksym;
    char       buf[64], *customkey;
    int        len;
    Rune       c;
    Status     status;
    Shortcut*  bp;

    if (IS_SET(MODE_KBDLOCK))
        return;

    if (xw.ime.xic)
        len = XmbLookupString(xw.ime.xic, e, buf, sizeof buf, &ksym, &status);
    else
        len = XLookupString(e, buf, sizeof buf, &ksym, NULL);
    /* 1. shortcuts */
    for (bp = shortcuts; bp < shortcuts + LEN(shortcuts); bp++)
    {
        if (ksym == bp->keysym && match(ljh::underlying_cast(bp->mod), e->state))
        {
            bp->func(bp->arg);
            return;
        }
    }

    /* 2. custom keys from config.h */
    if ((customkey = (char*)kmap(ksym, e->state)))
    {
        con.ttywrite(customkey, 1);
        return;
    }

    /* 3. composed string from input method */
    if (len == 0)
        return;
    if (len == 1 && e->state & Mod1Mask)
    {
        if (IS_SET(MODE_8BIT))
        {
            if (*buf < 0177)
            {
                c   = *buf | 0x80;
                len = utf8encode(c, buf);
            }
        }
        else
        {
            buf[1] = buf[0];
            buf[0] = '\033';
            len    = 2;
        }
    }
    con.ttywrite({buf, (size_t)len}, 1);
}

void cmessage(XEvent* e)
{
    /*
     * See xembed specs
     *  http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
     */
    if (e->xclient.message_type == xw.xembed && e->xclient.format == 32)
    {
        if (e->xclient.data.l[1] == XEMBED_FOCUS_IN)
        {
            win.mode |= MODE_FOCUSED;
            xseturgency(0);
        }
        else if (e->xclient.data.l[1] == XEMBED_FOCUS_OUT)
        {
            win.mode &= ~MODE_FOCUSED;
        }
    }
    else if (e->xclient.data.l[0] == xw.wmdeletewin)
    {
        con.ttyhangup();
        exit(0);
    }
}

void resize(XEvent* e)
{
    if (e->xconfigure.width == win.w && e->xconfigure.height == win.h)
        return;

    cresize(e->xconfigure.width, e->xconfigure.height);
}

void run(void)
{
    XEvent          ev;
    int             w = win.w, h = win.h;
    fd_set          rfd;
    int             xfd = XConnectionNumber(xw.dpy), xev, drawing;
    struct timespec seltv, *tv, now, lastblink, trigger;
    double          timeout;

    /* Waiting for window mapping */
    do
    {
        XNextEvent(xw.dpy, &ev);
        /*
         * This XFilterEvent call is required because of XOpenIM. It
         * does filter out the key event and some client message for
         * the input method too.
         */
        if (XFilterEvent(&ev, 0))
            continue;
        if (ev.type == ConfigureNotify)
        {
            w = ev.xconfigure.width;
            h = ev.xconfigure.height;
        }
    }
    while (ev.type != MapNotify);

    auto pty = con.ttynew(opt_line, shell, opt_io, opt_cmd);
    cresize(w, h);

    for (timeout = -1, drawing = 0, lastblink = (struct timespec){0};;)
    {
        FD_ZERO(&rfd);
        FD_SET(con.pty.output, &rfd);
        FD_SET(xfd, &rfd);

        if (XPending(xw.dpy) || con.ttyread_pending())
            timeout = 0; /* existing events might not set xfd */

        seltv.tv_sec  = timeout / 1E3;
        seltv.tv_nsec = 1E6 * (timeout - 1E3 * seltv.tv_sec);
        tv            = timeout >= 0 ? &seltv : NULL;

        if (pselect(MAX(xfd, con.pty.output) + 1, &rfd, NULL, NULL, tv, NULL) < 0)
        {
            if (errno == EINTR)
                continue;
            die("select failed: %s\n", strerror(errno));
        }
        clock_gettime(CLOCK_MONOTONIC, &now);

        int ttyin = FD_ISSET(con.pty.output, &rfd) || con.ttyread_pending();
        if (ttyin)
            con.ttyread();

        xev = 0;
        while (XPending(xw.dpy))
        {
            xev = 1;
            XNextEvent(xw.dpy, &ev);
            if (XFilterEvent(&ev, 0))
                continue;
            if (handler[ev.type])
                (handler[ev.type])(&ev);
        }

        /*
         * To reduce flicker and tearing, when new content or event
         * triggers drawing, we first wait a bit to ensure we got
         * everything, and if nothing new arrives - we draw.
         * We start with trying to wait minlatency ms. If more content
         * arrives sooner, we retry with shorter and shorter periods,
         * and eventually draw even without idle after maxlatency ms.
         * Typically this results in low latency while interacting,
         * maximum latency intervals during `cat huge.txt`, and perfect
         * sync with periodic updates from animations/key-repeats/etc.
         */
        if (ttyin || xev)
        {
            if (!drawing)
            {
                trigger = now;
                drawing = 1;
            }
            timeout = (maxlatency - TIMEDIFF(now, trigger)) / maxlatency * minlatency;
            if (timeout > 0)
                continue; /* we have time, try to find idle */
        }

        if (tinsync(su_timeout))
        {
            /*
             * on synchronized-update draw-suspension: don't reset
             * drawing so that we draw ASAP once we can (just after
             * ESU). it won't be too soon because we already can
             * draw now but we skip. we set timeout > 0 to draw on
             * SU-timeout even without new content.
             */
            timeout = minlatency;
            continue;
        }

        /* idle detected or maxlatency exhausted -> draw */
        timeout = -1;
        if (blinktimeout && con.tattrset(ATTR_BLINK))
        {
            timeout = blinktimeout - TIMEDIFF(now, lastblink);
            if (timeout <= 0)
            {
                if (-timeout > blinktimeout) /* start visible */
                    win.mode |= MODE_BLINK;
                win.mode ^= MODE_BLINK;
                con.tsetdirtattr(ATTR_BLINK);
                lastblink = now;
                timeout   = blinktimeout;
            }
        }

        draw();
        XFlush(xw.dpy);
        drawing = 0;
    }
}

void usage(void)
{
    die("usage: %s [-aiv] [-c class] [-f font] [-g geometry]"
        " [-n name] [-o file]\n"
        "          [-T title] [-t title] [-w windowid]"
        " [[-e] command [args ...]]\n"
        "       %s [-aiv] [-c class] [-f font] [-g geometry]"
        " [-n name] [-o file]\n"
        "          [-T title] [-t title] [-w windowid] -l line"
        " [stty_args ...]\n",
        argv0, argv0);
}

int main(int argc, char* argv[])
{
    xw.l = xw.t = 0;
    xw.isfixed  = False;
    xsetcursor(cursorshape);

    ARGBEGIN
    {
    case 'a':
        allowaltscreen = 0;
        break;
    case 'A':
        opt_alpha = EARGF(usage());
        break;
    case 'c':
        opt_class = EARGF(usage());
        break;
    case 'e':
        if (argc > 0)
            --argc, ++argv;
        goto run;
    case 'f':
        opt_font = EARGF(usage());
        break;
    case 'g':
        xw.gm = XParseGeometry(EARGF(usage()), &xw.l, &xw.t, &cols, &rows);
        break;
    case 'i':
        xw.isfixed = 1;
        break;
    case 'o':
        opt_io = EARGF(usage());
        break;
    case 'l':
        opt_line = EARGF(usage());
        break;
    case 'n':
        opt_name = EARGF(usage());
        break;
    case 't':
    case 'T':
        opt_title = EARGF(usage());
        break;
    case 'w':
        opt_embed = EARGF(usage());
        break;
    case 'v':
        die("%s " VERSION "\n", argv0);
        break;
    default:
        usage();
    }
    ARGEND;

run:
    if (argc > 0) /* eat all remaining arguments */
        opt_cmd = argv;

    if (!opt_title)
        opt_title = (char*)((opt_line || !opt_cmd) ? "st" : opt_cmd[0]);

    setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");
    cols = MAX(cols, 1);
    rows = MAX(rows, 1);
    con.tnew(cols, rows);
    xinit(cols, rows);
    xsetenv();
    con.selinit();
    run();

    return 0;
}
