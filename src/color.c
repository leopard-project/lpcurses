/*
 * lpcurses color-pair handling.
 *
 * Pair numbers are encoded in the classic ncurses A_COLOR bits; the backend
 * emits ANSI 16-color or xterm 256-color SGR sequences.
 */
#include "internal.h"

static struct {
    short fg;
    short bg;
} _pairs[256];

static unsigned char _pair_init[256];
static int _colors_started = 0;

void lp_color_reset(void)
{
    memset(_pairs, 0, sizeof(_pairs));
    memset(_pair_init, 0, sizeof(_pair_init));
    _pairs[0].fg = -1;
    _pairs[0].bg = -1;
    _pair_init[0] = 1;
    _colors_started = 0;
}

void lp_color_lookup(int pair, short *fg, short *bg)
{
    *fg = -1;
    *bg = -1;
    if (pair > 0 && pair < COLOR_PAIRS && _pair_init[pair]) {
        *fg = _pairs[pair].fg;
        *bg = _pairs[pair].bg;
    }
}

int start_color(void)
{
    _colors_started = 1;
    _pairs[0].fg = -1;
    _pairs[0].bg = -1;
    _pair_init[0] = 1;
    return OK;
}

bool has_colors(void)
{
    return true;
}

bool can_change_color(void)
{
    return false;
}

int init_pair(short pair, short f, short b)
{
    if (!_colors_started) return ERR;
    if (pair <= 0 || pair >= COLOR_PAIRS) return ERR;
    if (f < -1 || f >= COLORS) return ERR;
    if (b < -1 || b >= COLORS) return ERR;
    _pairs[pair].fg = f;
    _pairs[pair].bg = b;
    _pair_init[pair] = 1;
    return OK;
}

int pair_content(short pair, short *f, short *b)
{
    if (pair < 0 || pair >= COLOR_PAIRS) return ERR;
    if (f != NULL) *f = _pair_init[pair] ? _pairs[pair].fg : (short)-1;
    if (b != NULL) *b = _pair_init[pair] ? _pairs[pair].bg : (short)-1;
    return OK;
}

int use_default_colors(void)
{
    _pairs[0].fg = -1;
    _pairs[0].bg = -1;
    _pair_init[0] = 1;
    return OK;
}
