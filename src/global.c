/*
 * lpcurses global state.
 *
 * Public curses globals live here, along with the process-wide screen and
 * terminal state shared by the modules.  Per-window state is in WINDOW.
 */
#include "internal.h"

/* Public curses globals. */
WINDOW *stdscr = NULL;
WINDOW *curscr = NULL;

int LINES       = 0;
int COLS        = 0;
int COLORS      = 256;
int COLOR_PAIRS = 256;
int ESCDELAY    = 50;

/* Pending/refresh screen state. */
WINDOW *_newscr = NULL;
unsigned char *_newscr_force = NULL;
WINDOW *_winlist = NULL;
WINDOW *_last_win = NULL;

int _pending = 0;
int _clear_pending = 0;
int _resize_event = 0;
int _endwin = 0;
int _active = 0;

/* Cursor and ANSI terminal state. */
int _cursor_vis = 1;
int _term_cursor_visible = 1;

attr_t _term_attr = 0xffffffffU;
int    _term_alt = 0;
int    _phys_y = -1;
int    _phys_x = -1;

/* Saved terminal modes. */
struct termios _orig_termios;
int _have_termios = 0;
int _cbreak = 1;
int _raw = 0;
int _echo_mode = 1;
