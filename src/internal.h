/*
 * lpcurses internal shared definitions.
 *
 * This header is private to src/.  Public programs see include/curses.h only.
 */
#ifndef LPCURSES_INTERNAL_H
#define LPCURSES_INTERNAL_H

#include "curses.h"

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>

#ifndef TIOCGWINSZ
#define TIOCGWINSZ 0x5413
#endif

/* ------------------------------------------------------------------ */
/* Cell buffer                                                         */
/* ------------------------------------------------------------------ */

struct lpc_cell {
    uint32_t ch;    /* Unicode scalar; 0 marks the tail of a wide cell */
    attr_t   attr;  /* A_* bits + color pair */
};

#define CELL_INVALID 0xffffffffu
#define KEY_NONE     (-2)

/*
 * Private per-window state.  WINDOW itself keeps the ncurses-visible
 * geometry/attribute fields in curses.h; everything below is hidden.
 */
struct lpc_win_internal {
    struct lpc_cell *cells;
    WINDOW          *parent;
    WINDOW          *root;      /* window whose cell buffer is shared */
    WINDOW          *next;      /* internal list of all windows */
    int              owns;      /* owns cells */
    int              clear;     /* clearok() pending */
    int              force;     /* touchwin() pending */
    int              scroll;    /* scrollok() */
    int              use_keypad;
    int              leave;
    int              delay;     /* wtimeout(); -1 = blocking */
};

#define WIN(w) ((struct lpc_win_internal *)((w)->_internal))

/* ------------------------------------------------------------------ */
/* Global state (global.c)                                             */
/* ------------------------------------------------------------------ */

extern WINDOW *_newscr;              /* composed pending screen */
extern unsigned char *_newscr_force;
extern WINDOW *_winlist;             /* all live windows */
extern WINDOW *_last_win;

extern int _pending;
extern int _clear_pending;
extern int _resize_event;
extern int _endwin;
extern int _active;

extern int _cursor_vis;
extern int _term_cursor_visible;

extern attr_t _term_attr;
extern int    _term_alt;
extern int    _phys_y;
extern int    _phys_x;

extern struct termios _orig_termios;
extern int _have_termios;
extern int _cbreak;
extern int _raw;
extern int _echo_mode;

/* ------------------------------------------------------------------ */
/* Small utilities and cell helpers (util.c)                           */
/* ------------------------------------------------------------------ */

int  lp_isspace(int c);
int  lp_isdigit(int c);
int  lp_digit_value(int c);
long long lp_now_ms(void);

int  lp_cp_width(uint32_t cp);
int  lp_utf8_encode(uint32_t cp, char out[4]);
int  lp_utf8_decode(const unsigned char *s, int n, uint32_t *cp);

void lp_fill_cells(struct lpc_cell *cells, int count, uint32_t ch, attr_t attr);
void lp_blank_range(WINDOW *w, int y, int x0, int x1, attr_t attr);
void lp_cell_store(WINDOW *w, int y, int x, uint32_t cp, attr_t attr);

static inline attr_t lp_blank_attr(WINDOW *w)
{
    return w->_bkgd & A_ATTRIBUTES;
}

/* ------------------------------------------------------------------ */
/* Output buffer and ANSI emission (refresh.c)                         */
/* ------------------------------------------------------------------ */

void lp_out_flush(void);
void lp_out_mem(const char *s, size_t n);
void lp_out_str(const char *s);
void lp_out_ch(char c);
void lp_out_uint(unsigned int v);
void lp_out_cp(uint32_t cp);

/* ------------------------------------------------------------------ */
/* Window allocation / resize (window.c)                               */
/* ------------------------------------------------------------------ */

WINDOW *lp_window_alloc(int nlines, int ncols, int begy, int begx,
                        int own, WINDOW *root, struct lpc_cell *cells);
WINDOW *lp_screen_window_alloc(int rows, int cols);
void    lp_window_unlink(WINDOW *w);
int     lp_resize_screen(int rows, int cols);

/* ------------------------------------------------------------------ */
/* Terminal / screen (screen.c)                                        */
/* ------------------------------------------------------------------ */

int  lp_query_size(int *rows, int *cols);
void lp_apply_mode(void);
int  lp_ensure_active(void);
int  lp_detect_resize(void);

/* ------------------------------------------------------------------ */
/* Output helpers shared with box.c (output.c)                         */
/* ------------------------------------------------------------------ */

void lp_put_chtype_at(WINDOW *w, int y, int x, chtype ch);

/* ------------------------------------------------------------------ */
/* Colors (color.c)                                                    */
/* ------------------------------------------------------------------ */

void lp_color_reset(void);
void lp_color_lookup(int pair, short *fg, short *bg);

#endif /* LPCURSES_INTERNAL_H */
