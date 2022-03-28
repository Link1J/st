#pragma once
#include <cstdint>
#include <array>
#include <vector>

constexpr auto UTF_INVALID = 0xFFFD;
constexpr auto UTF_SIZ     = 4;
constexpr auto ESC_BUF_SIZ = (128 * UTF_SIZ);
constexpr auto ESC_ARG_SIZ = 16;
constexpr auto CAR_PER_ARG = 4;
constexpr auto STR_BUF_SIZ = ESC_BUF_SIZ;
constexpr auto STR_ARG_SIZ = ESC_ARG_SIZ;
constexpr auto HISTSIZE    = 2000;

using uchar  = unsigned char;
using uint   = unsigned int;
using ulong  = unsigned long;
using ushort = unsigned short;

using Rune = uint_least32_t;

struct Glyph
{
    Rune     u;         // character code
    ushort   mode;      // attribute flags
    uint32_t fg;        // foreground
    uint32_t bg;        // background
    int      ustyle;    // underline style
    int      ucolor[3]; // underline color
};

using Line = std::vector<Glyph>;

struct TCursor
{
    Glyph attr; // current char attributes
    int   x;
    int   y;
    char  state;
};

// Internal representation of the screen
struct Term
{
    int                        row;      // nb row
    int                        col;      // nb col
    std::vector<Line>          line;     // screen
    std::vector<Line>          alt;      // alternate screen
    std::array<Line, HISTSIZE> hist;     // history buffer
    int                        histi;    // history index
    int                        scr;      // scroll back
    std::vector<int>           dirty;    // dirtyness of lines
    TCursor                    c;        // cursor
    int                        ocx;      // old cursor col
    int                        ocy;      // old cursor row
    int                        top;      // top    scroll limit
    int                        bot;      // bottom scroll limit
    int                        mode;     // terminal mode flags
    int                        esc;      // escape state flags
    std::array<char, 4>        trantbl;  // charset table translation
    int                        charset;  // current charset
    int                        icharset; // selected charset for sequence
    std::vector<int>           tabs;     //
    Rune                       lastc;    // last printed char outside of sequence, 0 if control
};

static void    term_dumpsel(Term&);
static void    term_dumpline(Term&, int);
static void    term_dump(Term&);
static void    term_clearregion(Term&, int, int, int, int);
static void    term_cursor(Term&, int);
static void    term_deletechar(Term&, int);
static void    term_deleteline(Term&, int);
static void    term_insertblank(Term&, int);
static void    term_insertblankline(Term&, int);
static int     term_linelen(Term&, int);
static void    term_moveto(Term&, int, int);
static void    term_moveato(Term&, int, int);
static void    term_newline(Term&, int);
static void    term_puttab(Term&, int);
static void    term_putc(Term&, Rune);
static void    term_reset(Term&);
static void    term_scrollup(Term&, int, int, int);
static void    term_scrolldown(Term&, int, int, int);
static void    term_setattr(Term&, int*, int);
static void    term_setchar(Term&, Rune, Glyph*, int, int);
static void    term_setdirt(Term&, int, int);
static void    term_setscroll(Term&, int, int);
static void    term_swapscreen(Term&);
static void    term_setmode(Term&, int, int, int*, int);
static int     term_write(Term&, char const*, int, int);
static void    term_fulldirt(Term&);
static void    term_controlcode(Term&, uchar);
static void    term_dectest(Term&, char);
static void    term_defutf8(Term&, char);
static int32_t term_defcolor(Term&, int*, int*, int);
static void    term_deftran(Term&, char);
static void    term_strsequence(Term&, uchar);