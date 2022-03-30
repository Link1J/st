/* See LICENSE file for copyright and license details. */
#include "config_types.hpp"

/*
 * appearance
 *
 * font: see http://freedesktop.org/software/fontconfig/fontconfig-user.html
 */
static char const* font     = "Cascadia Code PL:pixelsize=12:antialias=true:autohint=false";
static int         borderpx = 0;

/*
 * What program is execed by st depends of these precedence rules:
 * 1: program passed with -e
 * 2: scroll and/or utmp
 * 3: SHELL environment variable
 * 4: value of shell in /etc/passwd
 * 5: value of shell in config.h
 */
static char const* shell = "/bin/sh";
char const*        utmp  = NULL;
/* scroll program: to enable use a string like "scroll" */
char const* scroll    = NULL;
char const* stty_args = "stty raw pass8 nl -echo -iexten -cstopb 38400";

/* identification sequence returned in DA and DECID */
char const* vtiden = "\033[?63;4";

/* Kerning / character bounding-box multipliers */
static float cwscale = 1.0;
static float chscale = 1.0;

/*
 * word delimiter string
 *
 * More advanced example: L" `'\"()[]{}"
 */
wchar_t const* worddelimiters = L" ";

/* selection timeouts (in milliseconds) */
static unsigned int doubleclicktimeout = 300;
static unsigned int tripleclicktimeout = 600;

/* alt screens */
int allowaltscreen = 0;

/* allow certain non-interactive (insecure) window operations such as:
   setting the clipboard text */
int allowwindowops = 1;

/*
 * draw latency range in ms - from new content/keypress/etc until drawing.
 * within this range, st draws when content stops arriving (idle). mostly it's
 * near minlatency, but it waits longer for slow updates to avoid partial draw.
 * low minlatency will tear/flicker more, as it can "detect" idle too early.
 */
static double minlatency = 8;
static double maxlatency = 33;

/*
 * Synchronized-Update timeout in ms
 * https://gitlab.com/gnachman/iterm2/-/wikis/synchronized-updates-spec
 */
static unsigned int su_timeout = 200;

/*
 * blinking timeout (set to 0 to disable blinking) for the terminal blinking
 * attribute.
 */
static unsigned int blinktimeout = 800;

/*
 * thickness of underline and bar cursors
 */
static unsigned int cursorthickness = 2;

/*
 * 1: render most of the lines/blocks characters without using the font for
 *    perfect alignment between cells (U2500 - U259F except dashes/diagonals).
 *    Bold affects lines thickness if boxdraw_bold is not 0. Italic is ignored.
 * 0: disable (render all U25XX glyphs normally from the font).
 */
int const boxdraw      = 1;
int const boxdraw_bold = 0;

/* braille (U28XX):  1: render as adjacent "pixels",  0: use font */
int const boxdraw_braille = 1;

/*
 * bell volume. It must be a value between -100 and 100. Use 0 for disabling
 * it
 */
static int bellvolume = 50;

/* default TERM value */
char const* termname = "st-256color";

/*
 * spaces per tab
 *
 * When you are changing this value, don't forget to adapt the »it« value in
 * the st.info and appropriately install the st.info in the environment where
 * you use this st version.
 *
 *	it#$tabspaces,
 *
 * Secondly make sure your kernel is not expanding tabs. When running `stty
 * -a` »tab0« should appear. You can tell the terminal to not expand tabs by
 *  running following command:
 *
 *	stty tabs
 */
unsigned int tabspaces = 4;

/* bg opacity */
float alpha = 0.9;

/* Terminal colors (16 first used in escape sequence) */
static char const* colorname[] = {
    /* 8 normal colors */
    "#202020", // black
    "#C50F1F", // red
    "#13A10E", // green
    "#C19C00", // yellow
    "#0037DA", // blue
    "#881798", // magenta
    "#3A96DD", // cyan
    "#CCCCCC", // white

    /* 8 bright colors */
    "#767676", // black
    "#E74856", // red
    "#16C60C", // green
    "#F9F1A5", // yellow
    "#3B78FF", // blue
    "#B4009E", // magenta
    "#61D6D6", // cyan
    "#FFFFFF", // white

    // clang-format off
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // clang-format on
    //[255] = 0,

    /* more colors can be added after 255 to use with DefaultXX */
    "#202020",
    "#404040",
    "#202020",
};
static_assert(std::size(colorname) >= 255);

/*
 * Default colors (colorname index)
 * foreground, background, cursor, reverse cursor
 */
unsigned int        defaultfg  = 15;
unsigned int        defaultbg  = 258;
static unsigned int defaultcs  = 256;
static unsigned int defaultrcs = 257;

/*
 * Default shape of cursor
 * 2: Block ("█")
 * 4: Underline ("_")
 * 6: Bar ("|")
 * 7: Snowman ("☃")
 */
static unsigned int cursorshape = 2;

/*
 * Default columns and rows numbers
 */

static unsigned int cols = 120;
static unsigned int rows = 41;

/*
 * Default shape of the mouse cursor
 */
static char const* mouseshape = "xterm";

/*
 * Color used to display font attributes when fontconfig selected a font which
 * doesn't match the ones requested.
 */
static unsigned int defaultattr = 11;

/*
 * Force mouse select/shortcuts while mask is active (when MODE_MOUSE is set).
 * Note that if you want to use MK::Shift with selmasks, set this to an other
 * modifier, set to 0 to not use it.
 */
static ModifierKeys forcemousemod = MK::Shift;

/*
 * Internal mouse shortcuts.
 * Beware that overloading Button1 will disable the selection.
 */
static MouseShortcut mshortcuts[] = {
    // clang-format off
    /* mask       button               function        argument     release */
    {  MK::Any  , MB::ScrollWheelUp  , kscrollup     , 1          , false   },
    {  MK::Any  , MB::ScrollWheelDown, kscrolldown   , 1          , false   },
    {  MK::Any  , MB::Middle         , selpaste      , 0          , true    },
    {  MK::Shift, MB::ScrollWheelUp  , ttysend       , "\033[5;2~", false   },
    {  MK::Any  , MB::ScrollWheelUp  , ttysend       , "\031"     , false   },
    {  MK::Shift, MB::ScrollWheelDown, ttysend       , "\033[6;2~", false   },
    {  MK::Any  , MB::ScrollWheelDown, ttysend       , "\005"     , false   },
    // clang-format on
};

/* Internal keyboard shortcuts. */
static Shortcut shortcuts[] = {
    // clang-format off
    /* mask         keysym         function       argument */
    {  MK::Any    , KC::Break    , sendbreak    ,  0       },
    {  MK::Control, KC::Print    , toggleprinter,  0       },
    {  MK::Shift  , KC::Print    , printscreen  ,  0       },
    {  MK::Any    , KC::Print    , printsel     ,  0       },
    {  MK::Term   , KC::Prior    , zoom         , +1       },
    {  MK::Term   , KC::Next     , zoom         , -1       },
    {  MK::Term   , KC::Home     , zoomreset    ,  0       },
    {  MK::Term   , KC::C        , clipcopy     ,  0       },
    {  MK::Term   , KC::V        , clippaste    ,  0       },
    {  MK::Term   , KC::Y        , selpaste     ,  0       },
    {  MK::Shift  , KC::Insert   , selpaste     ,  0       },
    {  MK::Term   , KC::Num_Lock , numlock      ,  0       },
    {  MK::Shift  , KC::Page_Up  , kscrollup    , -1       }, 
    {  MK::Shift  , KC::Page_Down, kscrolldown  , -1       },
    // clang-format on
};

// Special keys (change & recompile st.info accordingly)
//
// Mask value:
// - Use MK::Any to match the key no matter modifiers state
// - Use MK::None to match the key alone (no modifiers)
// appkey value:
// -    indeterminate: no value
// - >= true         : keypad application mode enabled
// - == double_true  : term.numlock = 1
// - <  false        : keypad application mode disabled
// appcursor value:
// -    indeterminate: no value
// - == true         : cursor application mode enabled
// - == false        : cursor application mode disabled
//
// Be careful with the order of the definitions because st searches in
// this table sequentially, so any MK::Any    must be in the last
// position for a key.

// If you want keys other than the X11 function keys (0xFD00 - 0xFFFF)
// to be mapped below, add them to this array.
static KeyCodes mappedkeys[] = {(KeyCodes)-1};

/*
 * This is the huge key array which defines all compatibility to the Linux
 * world. Please decide about changes wisely.
 */
static Key key[] = {
    // clang-format off
        /* keysym              mask                                 string        appkey         appcursor    */
        {  KC::NP_Home       , MK::Shift                          , "\033[2J"   , indeterminate, false        },
        {  KC::NP_Home       , MK::Shift                          , "\033[1;2H" , indeterminate, true         },
        {  KC::NP_Home       , MK::Any                            , "\033[H"    , indeterminate, false        },
        {  KC::NP_Home       , MK::Any                            , "\033[1~"   , indeterminate, true         },
        {  KC::NP_Up         , MK::Any                            , "\033Ox"    , true         , indeterminate},
        {  KC::NP_Up         , MK::Any                            , "\033[A"    , indeterminate, false        },
        {  KC::NP_Up         , MK::Any                            , "\033OA"    , indeterminate, true         },
        {  KC::NP_Down       , MK::Any                            , "\033Or"    , true         , indeterminate},
        {  KC::NP_Down       , MK::Any                            , "\033[B"    , indeterminate, false        },
        {  KC::NP_Down       , MK::Any                            , "\033OB"    , indeterminate, true         },
        {  KC::NP_Left       , MK::Any                            , "\033Ot"    , true         , indeterminate},
        {  KC::NP_Left       , MK::Any                            , "\033[D"    , indeterminate, false        },
        {  KC::NP_Left       , MK::Any                            , "\033OD"    , indeterminate, true         },
        {  KC::NP_Right      , MK::Any                            , "\033Ov"    , true         , indeterminate},
        {  KC::NP_Right      , MK::Any                            , "\033[C"    , indeterminate, false        },
        {  KC::NP_Right      , MK::Any                            , "\033OC"    , indeterminate, true         },
        {  KC::NP_Prior      , MK::Shift                          , "\033[5;2~" , indeterminate, indeterminate},
        {  KC::NP_Prior      , MK::Any                            , "\033[5~"   , indeterminate, indeterminate},
        {  KC::NP_Begin      , MK::Any                            , "\033[E"    , indeterminate, indeterminate},
        {  KC::NP_End        , MK::Control                        , "\033[J"    , false        , indeterminate},
        {  KC::NP_End        , MK::Control                        , "\033[1;5F" , true         , indeterminate},
        {  KC::NP_End        , MK::Shift                          , "\033[K"    , false        , indeterminate},
        {  KC::NP_End        , MK::Shift                          , "\033[1;2F" , true         , indeterminate},
        {  KC::NP_End        , MK::Any                            , "\033[4~"   , indeterminate, indeterminate},
        {  KC::NP_Next       , MK::Shift                          , "\033[6;2~" , indeterminate, indeterminate},
        {  KC::NP_Next       , MK::Any                            , "\033[6~"   , indeterminate, indeterminate},
        {  KC::NP_Insert     , MK::Shift                          , "\033[2;2~" , true         , indeterminate},
        {  KC::NP_Insert     , MK::Shift                          , "\033[4l"   , false        , indeterminate},
        {  KC::NP_Insert     , MK::Control                        , "\033[L"    , false        , indeterminate},
        {  KC::NP_Insert     , MK::Control                        , "\033[2;5~" , true         , indeterminate},
        {  KC::NP_Insert     , MK::Any                            , "\033[4h"   , false        , indeterminate},
        {  KC::NP_Insert     , MK::Any                            , "\033[2~"   , true         , indeterminate},
        {  KC::NP_Delete     , MK::Control                        , "\033[M"    , false        , indeterminate},
        {  KC::NP_Delete     , MK::Control                        , "\033[3;5~" , true         , indeterminate},
        {  KC::NP_Delete     , MK::Shift                          , "\033[2K"   , false        , indeterminate},
        {  KC::NP_Delete     , MK::Shift                          , "\033[3;2~" , true         , indeterminate},
        {  KC::NP_Delete     , MK::Any                            , "\033[3~"   , false        , indeterminate},
        {  KC::NP_Delete     , MK::Any                            , "\033[3~"   , true         , indeterminate},
        {  KC::NP_Multiply   , MK::Any                            , "\033Oj"    , double_true  , indeterminate},
        {  KC::NP_Add        , MK::Any                            , "\033Ok"    , double_true  , indeterminate},
        {  KC::NP_Enter      , MK::Any                            , "\033OM"    , double_true  , indeterminate},
        {  KC::NP_Enter      , MK::Any                            , "\r"        , false        , indeterminate},
        {  KC::NP_Subtract   , MK::Any                            , "\033Om"    , double_true  , indeterminate},
        {  KC::NP_Decimal    , MK::Any                            , "\033On"    , double_true  , indeterminate},
        {  KC::NP_Divide     , MK::Any                            , "\033Oo"    , double_true  , indeterminate},
        {  KC::NP_0          , MK::Any                            , "\033Op"    , double_true  , indeterminate},
        {  KC::NP_1          , MK::Any                            , "\033Oq"    , double_true  , indeterminate},
        {  KC::NP_2          , MK::Any                            , "\033Or"    , double_true  , indeterminate},
        {  KC::NP_3          , MK::Any                            , "\033Os"    , double_true  , indeterminate},
        {  KC::NP_4          , MK::Any                            , "\033Ot"    , double_true  , indeterminate},
        {  KC::NP_5          , MK::Any                            , "\033Ou"    , double_true  , indeterminate},
        {  KC::NP_6          , MK::Any                            , "\033Ov"    , double_true  , indeterminate},
        {  KC::NP_7          , MK::Any                            , "\033Ow"    , double_true  , indeterminate},
        {  KC::NP_8          , MK::Any                            , "\033Ox"    , double_true  , indeterminate},
        {  KC::NP_9          , MK::Any                            , "\033Oy"    , double_true  , indeterminate},
        {  KC::Up            , MK::Shift                          , "\033[1;2A" , indeterminate, indeterminate},
        {  KC::Up            , MK::Alt                            , "\033[1;3A" , indeterminate, indeterminate},
        {  KC::Up            , MK::Shift   | MK::Alt              , "\033[1;4A" , indeterminate, indeterminate},
        {  KC::Up            , MK::Control                        , "\033[1;5A" , indeterminate, indeterminate},
        {  KC::Up            , MK::Shift   | MK::Control          , "\033[1;6A" , indeterminate, indeterminate},
        {  KC::Up            , MK::Control | MK::Alt              , "\033[1;7A" , indeterminate, indeterminate},
        {  KC::Up            , MK::Shift   | MK::Control | MK::Alt, "\033[1;8A" , indeterminate, indeterminate},
        {  KC::Up            , MK::Any                            , "\033[A"    , indeterminate, false        },
        {  KC::Up            , MK::Any                            , "\033OA"    , indeterminate, true         },
        {  KC::Down          , MK::Shift                          , "\033[1;2B" , indeterminate, indeterminate},
        {  KC::Down          , MK::Alt                            , "\033[1;3B" , indeterminate, indeterminate},
        {  KC::Down          , MK::Shift   | MK::Alt              , "\033[1;4B" , indeterminate, indeterminate},
        {  KC::Down          , MK::Control                        , "\033[1;5B" , indeterminate, indeterminate},
        {  KC::Down          , MK::Shift   | MK::Control          , "\033[1;6B" , indeterminate, indeterminate},
        {  KC::Down          , MK::Control | MK::Alt              , "\033[1;7B" , indeterminate, indeterminate},
        {  KC::Down          , MK::Shift   | MK::Control | MK::Alt, "\033[1;8B" , indeterminate, indeterminate},
        {  KC::Down          , MK::Any                            , "\033[B"    , indeterminate, false        },
        {  KC::Down          , MK::Any                            , "\033OB"    , indeterminate, true         },
        {  KC::Left          , MK::Shift                          , "\033[1;2D" , indeterminate, indeterminate},
        {  KC::Left          , MK::Alt                            , "\033[1;3D" , indeterminate, indeterminate},
        {  KC::Left          , MK::Shift   | MK::Alt              , "\033[1;4D" , indeterminate, indeterminate},
        {  KC::Left          , MK::Control                        , "\033[1;5D" , indeterminate, indeterminate},
        {  KC::Left          , MK::Shift   | MK::Control          , "\033[1;6D" , indeterminate, indeterminate},
        {  KC::Left          , MK::Control | MK::Alt              , "\033[1;7D" , indeterminate, indeterminate},
        {  KC::Left          , MK::Shift   | MK::Control | MK::Alt, "\033[1;8D" , indeterminate, indeterminate},
        {  KC::Left          , MK::Any                            , "\033[D"    , indeterminate, false        },
        {  KC::Left          , MK::Any                            , "\033OD"    , indeterminate, true         },
        {  KC::Right         , MK::Shift                          , "\033[1;2C" , indeterminate, indeterminate},
        {  KC::Right         , MK::Alt                            , "\033[1;3C" , indeterminate, indeterminate},
        {  KC::Right         , MK::Shift   | MK::Alt              , "\033[1;4C" , indeterminate, indeterminate},
        {  KC::Right         , MK::Control                        , "\033[1;5C" , indeterminate, indeterminate},
        {  KC::Right         , MK::Shift   | MK::Control          , "\033[1;6C" , indeterminate, indeterminate},
        {  KC::Right         , MK::Control | MK::Alt              , "\033[1;7C" , indeterminate, indeterminate},
        {  KC::Right         , MK::Shift   | MK::Control | MK::Alt, "\033[1;8C" , indeterminate, indeterminate},
        {  KC::Right         , MK::Any                            , "\033[C"    , indeterminate, false        },
        {  KC::Right         , MK::Any                            , "\033OC"    , indeterminate, true         },
        {  KC::ISO_Left_Tab  , MK::Shift                          , "\033[Z"    , indeterminate, indeterminate},
        {  KC::Enter         , MK::Alt                            , "\033\r"    , indeterminate, indeterminate},
        {  KC::Enter         , MK::Any                            , "\r"        , indeterminate, indeterminate},
        {  KC::Insert        , MK::Shift                          , "\033[4l"   , false        , indeterminate},
        {  KC::Insert        , MK::Shift                          , "\033[2;2~" , true         , indeterminate},
        {  KC::Insert        , MK::Control                        , "\033[L"    , false        , indeterminate},
        {  KC::Insert        , MK::Control                        , "\033[2;5~" , true         , indeterminate},
        {  KC::Insert        , MK::Any                            , "\033[4h"   , false        , indeterminate},
        {  KC::Insert        , MK::Any                            , "\033[2~"   , true         , indeterminate},
        {  KC::Delete        , MK::Control                        , "\033[M"    , false        , indeterminate},
        {  KC::Delete        , MK::Control                        , "\033[3;5~" , true         , indeterminate},
        {  KC::Delete        , MK::Shift                          , "\033[2K"   , false        , indeterminate},
        {  KC::Delete        , MK::Shift                          , "\033[3;2~" , true         , indeterminate},
        {  KC::Delete        , MK::Any                            , "\033[3~"   , false        , indeterminate},
        {  KC::Delete        , MK::Any                            , "\033[3~"   , true         , indeterminate},
        {  KC::Backspace     , MK::None                           , "\177"      , indeterminate, indeterminate},
        {  KC::Backspace     , MK::Alt                            , "\033\177"  , indeterminate, indeterminate},
        {  KC::Home          , MK::Shift                          , "\033[2J"   , indeterminate, false        },
        {  KC::Home          , MK::Shift                          , "\033[1;2H" , indeterminate, true         },
        {  KC::Home          , MK::Any                            , "\033[H"    , indeterminate, false        },
        {  KC::Home          , MK::Any                            , "\033[1~"   , indeterminate, true         },
        {  KC::End           , MK::Control                        , "\033[J"    , false        , indeterminate},
        {  KC::End           , MK::Control                        , "\033[1;5F" , true         , indeterminate},
        {  KC::End           , MK::Shift                          , "\033[K"    , false        , indeterminate},
        {  KC::End           , MK::Shift                          , "\033[1;2F" , true         , indeterminate},
        {  KC::End           , MK::Any                            , "\033[4~"   , indeterminate, indeterminate},
        {  KC::Prior         , MK::Control                        , "\033[5;5~" , indeterminate, indeterminate},
        {  KC::Prior         , MK::Shift                          , "\033[5;2~" , indeterminate, indeterminate},
        {  KC::Prior         , MK::Any                            , "\033[5~"   , indeterminate, indeterminate},
        {  KC::Next          , MK::Control                        , "\033[6;5~" , indeterminate, indeterminate},
        {  KC::Next          , MK::Shift                          , "\033[6;2~" , indeterminate, indeterminate},
        {  KC::Next          , MK::Any                            , "\033[6~"   , indeterminate, indeterminate},
        {  KC::F1            , MK::None                           , "\033OP"    , indeterminate, indeterminate},
/*F13*/ {  KC::F1            , MK::Shift                          , "\033[1;2P" , indeterminate, indeterminate},
/*F25*/ {  KC::F1            , MK::Control                        , "\033[1;5P" , indeterminate, indeterminate},
/*F37*/ {  KC::F1            , MK::Win                            , "\033[1;6P" , indeterminate, indeterminate},
/*F49*/ {  KC::F1            , MK::Alt                            , "\033[1;3P" , indeterminate, indeterminate},
/*F61*/ {  KC::F1            , MK::Mod3                           , "\033[1;4P" , indeterminate, indeterminate},
        {  KC::F2            , MK::None                           , "\033OQ"    , indeterminate, indeterminate},
/*F14*/ {  KC::F2            , MK::Shift                          , "\033[1;2Q" , indeterminate, indeterminate},
/*F26*/ {  KC::F2            , MK::Control                        , "\033[1;5Q" , indeterminate, indeterminate},
/*F38*/ {  KC::F2            , MK::Win                            , "\033[1;6Q" , indeterminate, indeterminate},
/*F50*/ {  KC::F2            , MK::Alt                            , "\033[1;3Q" , indeterminate, indeterminate},
/*F62*/ {  KC::F2            , MK::Mod3                           , "\033[1;4Q" , indeterminate, indeterminate},
        {  KC::F3            , MK::None                           , "\033OR"    , indeterminate, indeterminate},
/*F15*/ {  KC::F3            , MK::Shift                          , "\033[1;2R" , indeterminate, indeterminate},
/*F27*/ {  KC::F3            , MK::Control                        , "\033[1;5R" , indeterminate, indeterminate},
/*F39*/ {  KC::F3            , MK::Win                            , "\033[1;6R" , indeterminate, indeterminate},
/*F51*/ {  KC::F3            , MK::Alt                            , "\033[1;3R" , indeterminate, indeterminate},
/*F63*/ {  KC::F3            , MK::Mod3                           , "\033[1;4R" , indeterminate, indeterminate},
        {  KC::F4            , MK::None                           , "\033OS"    , indeterminate, indeterminate},
/*F16*/ {  KC::F4            , MK::Shift                          , "\033[1;2S" , indeterminate, indeterminate},
/*F28*/ {  KC::F4            , MK::Control                        , "\033[1;5S" , indeterminate, indeterminate},
/*F40*/ {  KC::F4            , MK::Win                            , "\033[1;6S" , indeterminate, indeterminate},
/*F52*/ {  KC::F4            , MK::Alt                            , "\033[1;3S" , indeterminate, indeterminate},
        {  KC::F5            , MK::None                           , "\033[15~"  , indeterminate, indeterminate},
/*F17*/ {  KC::F5            , MK::Shift                          , "\033[15;2~", indeterminate, indeterminate},
/*F29*/ {  KC::F5            , MK::Control                        , "\033[15;5~", indeterminate, indeterminate},
/*F41*/ {  KC::F5            , MK::Win                            , "\033[15;6~", indeterminate, indeterminate},
/*F53*/ {  KC::F5            , MK::Alt                            , "\033[15;3~", indeterminate, indeterminate},
        {  KC::F6            , MK::None                           , "\033[17~"  , indeterminate, indeterminate},
/*F18*/ {  KC::F6            , MK::Shift                          , "\033[17;2~", indeterminate, indeterminate},
/*F30*/ {  KC::F6            , MK::Control                        , "\033[17;5~", indeterminate, indeterminate},
/*F42*/ {  KC::F6            , MK::Win                            , "\033[17;6~", indeterminate, indeterminate},
/*F54*/ {  KC::F6            , MK::Alt                            , "\033[17;3~", indeterminate, indeterminate},
        {  KC::F7            , MK::None                           , "\033[18~"  , indeterminate, indeterminate},
/*F19*/ {  KC::F7            , MK::Shift                          , "\033[18;2~", indeterminate, indeterminate},
/*F31*/ {  KC::F7            , MK::Control                        , "\033[18;5~", indeterminate, indeterminate},
/*F43*/ {  KC::F7            , MK::Win                            , "\033[18;6~", indeterminate, indeterminate},
/*F55*/ {  KC::F7            , MK::Alt                            , "\033[18;3~", indeterminate, indeterminate},
        {  KC::F8            , MK::None                           , "\033[19~"  , indeterminate, indeterminate},
/*F20*/ {  KC::F8            , MK::Shift                          , "\033[19;2~", indeterminate, indeterminate},
/*F32*/ {  KC::F8            , MK::Control                        , "\033[19;5~", indeterminate, indeterminate},
/*F44*/ {  KC::F8            , MK::Win                            , "\033[19;6~", indeterminate, indeterminate},
/*F56*/ {  KC::F8            , MK::Alt                            , "\033[19;3~", indeterminate, indeterminate},
        {  KC::F9            , MK::None                           , "\033[20~"  , indeterminate, indeterminate},
/*F21*/ {  KC::F9            , MK::Shift                          , "\033[20;2~", indeterminate, indeterminate},
/*F33*/ {  KC::F9            , MK::Control                        , "\033[20;5~", indeterminate, indeterminate},
/*F45*/ {  KC::F9            , MK::Win                            , "\033[20;6~", indeterminate, indeterminate},
/*F57*/ {  KC::F9            , MK::Alt                            , "\033[20;3~", indeterminate, indeterminate},
        {  KC::F10           , MK::None                           , "\033[21~"  , indeterminate, indeterminate},
/*F22*/ {  KC::F10           , MK::Shift                          , "\033[21;2~", indeterminate, indeterminate},
/*F34*/ {  KC::F10           , MK::Control                        , "\033[21;5~", indeterminate, indeterminate},
/*F46*/ {  KC::F10           , MK::Win                            , "\033[21;6~", indeterminate, indeterminate},
/*F58*/ {  KC::F10           , MK::Alt                            , "\033[21;3~", indeterminate, indeterminate},
        {  KC::F11           , MK::None                           , "\033[23~"  , indeterminate, indeterminate},
/*F23*/ {  KC::F11           , MK::Shift                          , "\033[23;2~", indeterminate, indeterminate},
/*F35*/ {  KC::F11           , MK::Control                        , "\033[23;5~", indeterminate, indeterminate},
/*F47*/ {  KC::F11           , MK::Win                            , "\033[23;6~", indeterminate, indeterminate},
/*F59*/ {  KC::F11           , MK::Alt                            , "\033[23;3~", indeterminate, indeterminate},
        {  KC::F12           , MK::None                           , "\033[24~"  , indeterminate, indeterminate},
/*F24*/ {  KC::F12           , MK::Shift                          , "\033[24;2~", indeterminate, indeterminate},
/*F36*/ {  KC::F12           , MK::Control                        , "\033[24;5~", indeterminate, indeterminate},
/*F48*/ {  KC::F12           , MK::Win                            , "\033[24;6~", indeterminate, indeterminate},
/*F60*/ {  KC::F12           , MK::Alt                            , "\033[24;3~", indeterminate, indeterminate},
        {  KC::F13           , MK::None                           , "\033[1;2P" , indeterminate, indeterminate},
        {  KC::F14           , MK::None                           , "\033[1;2Q" , indeterminate, indeterminate},
        {  KC::F15           , MK::None                           , "\033[1;2R" , indeterminate, indeterminate},
        {  KC::F16           , MK::None                           , "\033[1;2S" , indeterminate, indeterminate},
        {  KC::F17           , MK::None                           , "\033[15;2~", indeterminate, indeterminate},
        {  KC::F18           , MK::None                           , "\033[17;2~", indeterminate, indeterminate},
        {  KC::F19           , MK::None                           , "\033[18;2~", indeterminate, indeterminate},
        {  KC::F20           , MK::None                           , "\033[19;2~", indeterminate, indeterminate},
        {  KC::F21           , MK::None                           , "\033[20;2~", indeterminate, indeterminate},
        {  KC::F22           , MK::None                           , "\033[21;2~", indeterminate, indeterminate},
        {  KC::F23           , MK::None                           , "\033[23;2~", indeterminate, indeterminate},
        {  KC::F24           , MK::None                           , "\033[24;2~", indeterminate, indeterminate},
        {  KC::F25           , MK::None                           , "\033[1;5P" , indeterminate, indeterminate},
        {  KC::F26           , MK::None                           , "\033[1;5Q" , indeterminate, indeterminate},
        {  KC::F27           , MK::None                           , "\033[1;5R" , indeterminate, indeterminate},
        {  KC::F28           , MK::None                           , "\033[1;5S" , indeterminate, indeterminate},
        {  KC::F29           , MK::None                           , "\033[15;5~", indeterminate, indeterminate},
        {  KC::F30           , MK::None                           , "\033[17;5~", indeterminate, indeterminate},
        {  KC::F31           , MK::None                           , "\033[18;5~", indeterminate, indeterminate},
        {  KC::F32           , MK::None                           , "\033[19;5~", indeterminate, indeterminate},
        {  KC::F33           , MK::None                           , "\033[20;5~", indeterminate, indeterminate},
        {  KC::F34           , MK::None                           , "\033[21;5~", indeterminate, indeterminate},
        {  KC::F35           , MK::None                           , "\033[23;5~", indeterminate, indeterminate},
    // clang-format on
};

/*
 * Selection types' masks.
 * Use the same masks as usual.
 * Button1Mask is always unset, to make masks match between ButtonPress.
 * ButtonRelease and MotionNotify.
 * If no match is found, regular selection is used.
 */
static ModifierKeys selmasks[] = {
    MK::None,
    MK::None,
    /*[SEL_RECTANGULAR] =*/MK::Alt,
};

/*
 * Printable characters in ASCII, used to estimate the advance width
 * of single wide characters.
 */
static char ascii_printable[] = " !\"#$%&'()*+,-./0123456789:;<=>?"
                                "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                                "`abcdefghijklmnopqrstuvwxyz{|}~";

/**
 * Undercurl style. Set UNDERCURL_STYLE to one of the available styles.
 *
 * Curly: Dunno how to draw it *shrug*
 *  _   _   _   _
 * ( ) ( ) ( ) ( )
 *	 (_) (_) (_) (_)
 *
 * Spiky:
 * /\  /\   /\	/\
 *   \/  \/	  \/
 *
 * Capped:
 *	_     _     _
 * / \   / \   / \
 *    \_/   \_/
 */
// Available styles
#define UNDERCURL_CURLY 0
#define UNDERCURL_SPIKY 1
#define UNDERCURL_CAPPED 2
// Active style
#define UNDERCURL_STYLE UNDERCURL_SPIKY
