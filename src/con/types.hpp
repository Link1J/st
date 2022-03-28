#pragma once
#include "../support.hpp"
#include "../utf8.hpp"
#include "enum.hpp"

constexpr auto ESC_BUF_SIZ = (128 * UTF_SIZ);
constexpr auto ESC_ARG_SIZ = 16;
constexpr auto CAR_PER_ARG = 4;
constexpr auto STR_BUF_SIZ = ESC_BUF_SIZ;
constexpr auto STR_ARG_SIZ = ESC_ARG_SIZ;
constexpr auto HISTSIZE    = 2000;

#include <vector>
#include <array>
#include <string>
#include <string_view>

#if !defined(_WIN32)
#include <pwd.h>
#if defined(__linux)
#include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <util.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <libutil.h>
#endif

using pipe_t = int;

#else
#include <Windows.h>
#include <winrt/base.h>

using pipe_t = winrt::handle;
using pid_t  = winrt::handle;

namespace winrt
{
    struct hpcon_traits
    {
        using type = HPCON;

        static void close(type value) noexcept
        {
            ClosePseudoConsole(value);
        }

        static constexpr type invalid() noexcept
        {
            return nullptr;
        }
    };

    using hpcon = handle_type<hpcon_traits>;
} // namespace winrt

#endif

struct Glyph
{
    Rune                  u;          // character code
    ushort                mode;       // attribute flags
    uint32_t              fg;         // foreground
    uint32_t              bg;         // background
    int                   ustyle;     // underline style
    int                   ucolor[3];  // underline color
    std::vector<uint32_t> sixel_data; // sixel data
};

using Line = std::vector<Glyph>;

struct TCursor
{
    Glyph attr; // current char attributes
    int   x;
    int   y;
    char  state;
};

struct Selection
{
    int mode;
    int type;
    int snap;

    /*
     * Selection variables:
     * nb – normalized coordinates of the beginning of the selection
     * ne – normalized coordinates of the end of the selection
     * ob – original coordinates of the beginning of the selection
     * oe – original coordinates of the end of the selection
     */
    struct
    {
        int x, y;
    } nb, ne, ob, oe;

    int alt;
};

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

// CSI Escape sequence structs
// ESC '[' [[ [<priv>] <arg> [;]] <mode> [<mode>]]
struct CSIEscape
{
    std::array<char, ESC_BUF_SIZ>                         buf;  // raw string
    size_t                                                len;  // raw string length
    char                                                  priv; //
    std::array<int, ESC_ARG_SIZ>                          arg;  //
    int                                                   narg; // nb of args
    std::array<char, 2>                                   mode; //
    std::array<std::array<int, ESC_ARG_SIZ>, CAR_PER_ARG> carg; // colon args
};

// STR Escape sequence structs
// ESC type [[ [<priv>] <arg> [;]] <mode>] ESC '\'
struct STREscape
{
    char                           type; // ESC type ...
    char*                          buf;  // allocated raw string
    size_t                         siz;  // allocation size
    size_t                         len;  // raw string length
    std::array<char*, STR_ARG_SIZ> argp; // pointers to the end of nth argument
    int                            narg; // nb of args
};

struct Pty
{
    pid_t  process;
    pipe_t output;

#if defined(_WIN32)
    pipe_t       input;
    winrt::hpcon pc;
#endif
};