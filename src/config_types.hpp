#pragma once
#include <functional>
#include <variant>
#include <string_view>
#include <climits>

#include <ljh/bitmask_operators.hpp>

#include "quadbool.hpp"

#if defined(_WIN32)
#include <Windows.h>
enum class MouseButtons
{
    Left             = VK_LBUTTON,
    Middle           = VK_MBUTTON,
    Right            = VK_RBUTTON,
    Extra1           = VK_XBUTTON1,
    Extra2           = VK_XBUTTON2,
    ScrollWheelUp    = 4,
    ScrollWheelDown  = 5,
    ScrollWheelLeft  = 6,
    ScrollWheelRight = 7,
};
enum class KeyCodes
{
    // Letter keys
    A = 'A',
    B = 'B',
    C = 'C',
    D = 'D',
    E = 'E',
    F = 'F',
    G = 'G',
    H = 'H',
    I = 'I',
    J = 'J',
    K = 'K',
    L = 'L',
    M = 'M',
    N = 'N',
    O = 'O',
    P = 'P',
    Q = 'Q',
    R = 'R',
    S = 'S',
    T = 'T',
    U = 'U',
    V = 'V',
    W = 'W',
    X = 'X',
    Y = 'Y',
    Z = 'Z',

    // Number keys
    KB_1 = '1',
    KB_2 = '2',
    KB_3 = '3',
    KB_4 = '4',
    KB_5 = '5',
    KB_6 = '6',
    KB_7 = '7',
    KB_8 = '8',
    KB_9 = '9',
    KB_0 = '0',

    // Other input keys
    Enter     = VK_RETURN,
    Tab       = VK_TAB,
    Backspace = VK_BACK,
    Clear     = VK_CLEAR,
    Delete    = VK_DELETE,
    Space     = VK_SPACE,
    Insert    = VK_INSERT,
    Print     = VK_SNAPSHOT,
    Break     = VK_CANCEL,
    Num_Lock  = VK_NUMLOCK,

    // Cursor control
    Home         = VK_HOME,
    End          = VK_END,
    Left         = VK_LEFT,
    Up           = VK_UP,
    Right        = VK_RIGHT,
    Down         = VK_DOWN,
    Page_Up      = VK_PRIOR,
    Prior        = Page_Up,
    Page_Down    = VK_NEXT,
    Next         = Page_Down,
    Begin        = 0xFF58,
    ISO_Left_Tab = 0xFF59,

    // Function keys
    F1  = VK_F1,
    F2  = VK_F2,
    F3  = VK_F3,
    F4  = VK_F4,
    F5  = VK_F5,
    F6  = VK_F6,
    F7  = VK_F7,
    F8  = VK_F8,
    F9  = VK_F9,
    F10 = VK_F10,
    F11 = VK_F11,
    F12 = VK_F12,
    F13 = VK_F13,
    F14 = VK_F14,
    F15 = VK_F15,
    F16 = VK_F16,
    F17 = VK_F17,
    F18 = VK_F18,
    F19 = VK_F19,
    F20 = VK_F20,
    F21 = VK_F21,
    F22 = VK_F22,
    F23 = VK_F23,
    F24 = VK_F24,
    F25 = 0xFF88,
    F26 = 0xFF89,
    F27 = 0xFF8A,
    F28 = 0xFF8B,
    F29 = 0xFF8C,
    F30 = 0xFF8D,
    F31 = 0xFF8E,
    F32 = 0xFF8F,
    F33 = 0xFF90,
    F34 = 0xFF91,
    F35 = 0xFF92,

    // Numpad keys (numbers)
    NP_0 = VK_NUMPAD0,
    NP_1 = VK_NUMPAD1,
    NP_2 = VK_NUMPAD2,
    NP_3 = VK_NUMPAD3,
    NP_4 = VK_NUMPAD4,
    NP_5 = VK_NUMPAD5,
    NP_6 = VK_NUMPAD6,
    NP_7 = VK_NUMPAD7,
    NP_8 = VK_NUMPAD8,
    NP_9 = VK_NUMPAD9,

    // Numpad keys (other)
    NP_Space     = 0xFF01,
    NP_Tab       = 0xFF02,
    NP_Enter     = 0xFF03,
    NP_Home      = 0xFF04,
    NP_Left      = 0xFF05,
    NP_Up        = 0xFF06,
    NP_Right     = 0xFF07,
    NP_Down      = 0xFF08,
    NP_Prior     = 0xFF09,
    NP_Page_Up   = 0xFF0A,
    NP_Next      = 0xFF0B,
    NP_Page_Down = 0xFF0C,
    NP_End       = 0xFF0D,
    NP_Begin     = 0xFF0E,
    NP_Insert    = 0xFF0F,
    NP_Delete    = 0xFF10,
    NP_Equal     = 0xFF11,
    NP_Multiply  = VK_MULTIPLY,
    NP_Add       = VK_ADD,
    NP_Separator = VK_SEPARATOR,
    NP_Subtract  = VK_SUBTRACT,
    NP_Decimal   = VK_DECIMAL,
    NP_Divide    = VK_DIVIDE,
};
enum class ModifierKeys
{
    Control = 1 << 0,
    Shift   = 1 << 1,
    Alt     = 1 << 2,
    Win     = 1 << 3,
    Mod3    = 1 << 4,

    Ignore = 1 << 5,
    Term   = Control | Shift,

    None = 0,
    Any  = UINT_MAX,
};
#else /* assume X11 */
#include <X11/X.h>
#include <X11/keysym.h>
#undef None
enum class MouseButtons
{
    Left             = Button1,
    Middle           = Button2,
    Right            = Button3,
    ScrollWheelUp    = Button4,
    ScrollWheelDown  = Button5,
    ScrollWheelLeft  = 6,
    ScrollWheelRight = 7,
    Extra1           = 8,
    Extra2           = 9,
};
enum KeyCodes
{
    // Letter keys
    A = XK_A,
    B = XK_B,
    C = XK_C,
    D = XK_D,
    E = XK_E,
    F = XK_F,
    G = XK_G,
    H = XK_H,
    I = XK_I,
    J = XK_J,
    K = XK_K,
    L = XK_L,
    M = XK_M,
    N = XK_N,
    O = XK_O,
    P = XK_P,
    Q = XK_Q,
    R = XK_R,
    S = XK_S,
    T = XK_T,
    U = XK_U,
    V = XK_V,
    W = XK_W,
    X = XK_X,
    Y = XK_Y,
    Z = XK_Z,

    // Number keys
    KB_1 = XK_1,
    KB_2 = XK_2,
    KB_3 = XK_3,
    KB_4 = XK_4,
    KB_5 = XK_5,
    KB_6 = XK_6,
    KB_7 = XK_7,
    KB_8 = XK_8,
    KB_9 = XK_9,
    KB_0 = XK_0,

    // Other keys
    Enter     = XK_Return,
    Tab       = XK_Tab,
    Backspace = XK_BackSpace,
    Clear     = XK_Clear,
    Delete    = XK_Delete,
    Space     = XK_space,
    Insert    = XK_Insert,
    Print     = XK_Print,
    Break     = XK_Break,
    Num_Lock  = XK_Num_Lock,

    // Cursor control
    Home         = XK_Home,
    End          = XK_End,
    Left         = XK_Left,
    Up           = XK_Up,
    Right        = XK_Right,
    Down         = XK_Down,
    Page_Up      = XK_Page_Up,
    Prior        = XK_Prior,
    Page_Down    = XK_Page_Down,
    Next         = XK_Next,
    Begin        = XK_Begin,
    ISO_Left_Tab = XK_ISO_Left_Tab,

    // Function keys
    F1  = XK_F1,
    F2  = XK_F2,
    F3  = XK_F3,
    F4  = XK_F4,
    F5  = XK_F5,
    F6  = XK_F6,
    F7  = XK_F7,
    F8  = XK_F8,
    F9  = XK_F9,
    F10 = XK_F10,
    F11 = XK_F11,
    F12 = XK_F12,
    F13 = XK_F13,
    F14 = XK_F14,
    F15 = XK_F15,
    F16 = XK_F16,
    F17 = XK_F17,
    F18 = XK_F18,
    F19 = XK_F19,
    F20 = XK_F20,
    F21 = XK_F21,
    F22 = XK_F22,
    F23 = XK_F23,
    F24 = XK_F24,
    F25 = XK_F25,
    F26 = XK_F26,
    F27 = XK_F27,
    F28 = XK_F28,
    F29 = XK_F29,
    F30 = XK_F30,
    F31 = XK_F31,
    F32 = XK_F32,
    F33 = XK_F33,
    F34 = XK_F34,
    F35 = XK_F35,

    // Numpad keys (numbers)
    NP_0 = XK_KP_0,
    NP_1 = XK_KP_1,
    NP_2 = XK_KP_2,
    NP_3 = XK_KP_3,
    NP_4 = XK_KP_4,
    NP_5 = XK_KP_5,
    NP_6 = XK_KP_6,
    NP_7 = XK_KP_7,
    NP_8 = XK_KP_8,
    NP_9 = XK_KP_9,

    // Numpad keys (other)
    NP_Space     = XK_KP_Space,
    NP_Tab       = XK_KP_Tab,
    NP_Enter     = XK_KP_Enter,
    NP_Home      = XK_KP_Home,
    NP_Left      = XK_KP_Left,
    NP_Up        = XK_KP_Up,
    NP_Right     = XK_KP_Right,
    NP_Down      = XK_KP_Down,
    NP_Prior     = XK_KP_Prior,
    NP_Page_Up   = XK_KP_Page_Up,
    NP_Next      = XK_KP_Next,
    NP_Page_Down = XK_KP_Page_Down,
    NP_End       = XK_KP_End,
    NP_Begin     = XK_KP_Begin,
    NP_Insert    = XK_KP_Insert,
    NP_Delete    = XK_KP_Delete,
    NP_Equal     = XK_KP_Equal,
    NP_Multiply  = XK_KP_Multiply,
    NP_Add       = XK_KP_Add,
    NP_Separator = XK_KP_Separator,
    NP_Subtract  = XK_KP_Subtract,
    NP_Decimal   = XK_KP_Decimal,
    NP_Divide    = XK_KP_Divide,
};
enum class ModifierKeys : unsigned int
{
    Control = ControlMask,
    Shift   = ShiftMask,
    Alt     = Mod1Mask,
    Win     = Mod4Mask,
    Mod3    = Mod3Mask,

    Ignore = Mod2Mask | (0b11 << 13),
    Term   = Control | Shift,

    None = 0,
    Any  = UINT_MAX,
};
#endif

using KC = KeyCodes;
using MB = MouseButtons;
using MK = ModifierKeys;

template<>
struct ljh::bitmask_operators::enable<ModifierKeys> : std::true_type
{};

using Arg         = std::variant<int, unsigned int, double, char const*>;
using ArgCallback = std::function<void(Arg const&)>;

struct Shortcut
{
    ModifierKeys mod;
    KeyCodes     keysym;
    ArgCallback  func;
    const Arg    arg;
};

struct MouseShortcut
{
    ModifierKeys mod;
    MouseButtons button;
    ArgCallback  func;
    const Arg    arg;
    bool         release;
};

struct Key
{
    KeyCodes         k;         //
    ModifierKeys     mask;      //
    std::string_view s;         //
    qool             appkey;    // application keypad
    qool             appcursor; // application cursor
};

void redraw(void);
void draw(void);

void kscrolldown(Arg const&);
void kscrollup(Arg const&);
void printscreen(Arg const&);
void printsel(Arg const&);
void sendbreak(Arg const&);
void toggleprinter(Arg const&);

void resettitle(void);

void clipcopy(Arg const&);
void clippaste(Arg const&);
void numlock(Arg const&);
void selpaste(Arg const&);
void zoom(Arg const&);
void zoomabs(Arg const&);
void zoomreset(Arg const&);
void ttysend(Arg const&);
