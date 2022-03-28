#pragma once

enum selection_mode
{
    SEL_IDLE  = 0,
    SEL_EMPTY = 1,
    SEL_READY = 2
};

enum selection_type
{
    SEL_REGULAR     = 1,
    SEL_RECTANGULAR = 2
};

enum selection_snap
{
    SNAP_WORD = 1,
    SNAP_LINE = 2
};

enum glyph_attribute
{
    ATTR_NULL           = 0,
    ATTR_BOLD           = 1 << 0,
    ATTR_FAINT          = 1 << 1,
    ATTR_ITALIC         = 1 << 2,
    ATTR_UNDERLINE      = 1 << 3,
    ATTR_BLINK          = 1 << 4,
    ATTR_REVERSE        = 1 << 5,
    ATTR_INVISIBLE      = 1 << 6,
    ATTR_STRUCK         = 1 << 7,
    ATTR_WRAP           = 1 << 8,
    ATTR_WIDE           = 1 << 9,
    ATTR_WDUMMY         = 1 << 10,
    ATTR_BOXDRAW        = 1 << 11,
    ATTR_LIGA           = 1 << 12,
    ATTR_DIRTYUNDERLINE = 1 << 15,
    ATTR_BOLD_FAINT     = ATTR_BOLD | ATTR_FAINT,
};

enum mode
{
    MODE_WRAP      = 1 << 0,
    MODE_INSERT    = 1 << 1,
    MODE_ALTSCREEN = 1 << 2,
    MODE_CRLF      = 1 << 3,
    MODE_ECHO      = 1 << 4,
    MODE_PRINT     = 1 << 5,
    MODE_UTF8      = 1 << 6,
};

enum cursor_movement
{
    CURSOR_SAVE,
    CURSOR_LOAD
};

enum cursor_state
{
    CURSOR_DEFAULT  = 0,
    CURSOR_WRAPNEXT = 1,
    CURSOR_ORIGIN   = 2
};

enum escape_state
{
    ESC_START      = 1,  //
    ESC_CSI        = 2,  //
    ESC_STR        = 4,  // DCS, OSC, PM, APC
    ESC_ALTCHARSET = 8,  //
    ESC_STR_END    = 16, // a final string was encountered
    ESC_TEST       = 32, // Enter in test mode
    ESC_UTF8       = 64, //
};

enum charset
{
    CS_GRAPHIC0,
    CS_GRAPHIC1,
    CS_UK,
    CS_USA,
    CS_MULTI,
    CS_GER,
    CS_FIN
};