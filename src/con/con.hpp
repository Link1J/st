#pragma once
#include "types.hpp"
#include "enum.hpp"

#include <cwchar>

struct Con
{
    Pty       pty;
    Term      term;
    Selection sel;
    CSIEscape csiescseq;
    STREscape strescseq;

    int twrite_aborted = 0;
    int su             = 0;
    int iofd           = 0;

    void   ttyhangup(void);
    int    ttynew(char*, char const*, char*, char**);
    int    ttyread_pending();
    size_t ttyread(void);
    void   ttyresize(int, int);
    void   ttywrite(std::string_view, int);
    void   ttywriteraw(std::string_view);

    void  selclear(void);
    void  selinit(void);
    void  selstart(int, int, int);
    void  selextend(int, int, int, int);
    int   selected(int, int);
    char* getsel(void);
    void  selnormalize(void);
    void  selscroll(int, int);
    void  selsnap(int*, int*, int);

    void csidump(void);
    void csihandle(void);
    void readcolonargs(char**, int, std::array<std::array<int, ESC_ARG_SIZ>, CAR_PER_ARG>&);
    void csiparse(void);
    void csireset(void);
    int  eschandle(uchar);
    void strdump(void);
    void strhandle(void);
    void strparse(void);
    void strreset(void);

    int     tattrset(int);
    void    tnew(int, int);
    void    tresize(int, int);
    void    tsetdirtattr(int);
    void    tprinter(char const*, size_t);
    void    tdumpsel(void);
    void    tdumpline(int);
    void    tdump(void);
    void    tclearregion(int, int, int, int);
    void    tcursor(int);
    void    tdeletechar(int);
    void    tdeleteline(int);
    void    tinsertblank(int);
    void    tinsertblankline(int);
    int     tlinelen(int);
    void    tmoveto(int, int);
    void    tmoveato(int, int);
    void    tnewline(int);
    void    tputtab(int);
    void    tputc(Rune);
    void    treset(void);
    void    tscrollup(int, int, int);
    void    tscrolldown(int, int, int);
    void    tsetattr(int*, int);
    void    tsetchar(Rune, Glyph*, int, int);
    void    tsetdirt(int, int);
    void    tsetscroll(int, int);
    void    tswapscreen(void);
    void    tsetmode(int, int, int*, int);
    int     twrite(std::string_view, int);
    void    tfulldirt(void);
    void    tcontrolcode(uchar);
    void    tdectest(char);
    void    tdefutf8(char);
    int32_t tdefcolor(int*, int*, int);
    void    tdeftran(char);
    void    tstrsequence(uchar);
};

#define IS_SET(flag) ((term.mode & (flag)) != 0)
#define ISCONTROLC0(c) (BETWEEN(c, 0, 0x1f) || (c) == 0x7f)
#define ISCONTROLC1(c) (BETWEEN(c, 0x80, 0x9f))
#define ISCONTROL(c) (ISCONTROLC0(c) || ISCONTROLC1(c))
#define ISDELIM(u) (u && wcschr(worddelimiters, u))
#define TLINE(y) ((y) < term.scr ? term.hist[((y) + term.histi - term.scr + HISTSIZE + 1) % HISTSIZE] : term.line[(y)-term.scr])
#define STRESCARGREST(n) ((n) == 0 ? strescseq.buf.data() : strescseq.argp[(n)-1] + 1)
#define STRESCARGJUST(n) (*(strescseq.argp[n]) = '\0', STRESCARGREST(n))
#define ATTRCMP(a, b) (((a).mode & (~ATTR_WRAP) & (~ATTR_LIGA)) != ((b).mode & (~ATTR_WRAP) & (~ATTR_LIGA)) || (a).fg != (b).fg || (a).bg != (b).bg)

int    isboxdraw(Rune);
ushort boxdrawindex(Glyph const*);

/* config.h globals */
extern char const*    utmp;
extern char const*    scroll;
extern char const*    stty_args;
extern char const*    vtiden;
extern wchar_t const* worddelimiters;
extern int            allowaltscreen;
extern int            allowwindowops;
extern char const*    termname;
extern unsigned int   tabspaces;
extern unsigned int   defaultfg;
extern unsigned int   defaultbg;
extern float          alpha;
extern int const      boxdraw, boxdraw_bold, boxdraw_braille;

#if defined(_WIN32)
extern "C" int wcwidth(wchar_t);
#endif