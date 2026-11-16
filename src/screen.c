/*
 * lpcurses terminal lifecycle, termios save/restore, alternate screen,
 * cursor visibility and screen-resize detection.
 */
#include "internal.h"

int lp_query_size(int *rows, int *cols)
{
    struct winsize ws;

    memset(&ws, 0, sizeof(ws));
    if (ioctl(1, TIOCGWINSZ, &ws) != 0 &&
        ioctl(0, TIOCGWINSZ, &ws) != 0)
        return ERR;
    if (ws.ws_row == 0 || ws.ws_col == 0) return ERR;
    *rows = (int)ws.ws_row;
    *cols = (int)ws.ws_col;
    return OK;
}

void lp_apply_mode(void)
{
    struct termios t;

    if (!_have_termios) return;
    t = _orig_termios;

    /* The tty driver never echoes; curses echo is implemented in getch(). */
    t.c_lflag &= ~(uint32_t)(ECHO | ECHONL);

    if (_raw) {
        t.c_iflag &= ~(uint32_t)(IXON | ICRNL | BRKINT | INPCK | ISTRIP |
                                 INLCR | IGNCR | IXOFF);
        t.c_oflag &= ~(uint32_t)OPOST;
        t.c_lflag &= ~(uint32_t)(ICANON | ISIG | IEXTEN);
        t.c_cflag |= (uint32_t)CS8;
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
    } else if (_cbreak) {
        t.c_lflag &= ~(uint32_t)ICANON;
        t.c_lflag |= (uint32_t)ISIG;
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
    } else {
        t.c_lflag |= (uint32_t)ICANON;
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
    }

    (void)tcsetattr(0, TCSANOW, &t);
}

int lp_ensure_active(void)
{
    if (stdscr == NULL) return ERR;
    if (!_endwin) return OK;

    _endwin = 0;
    _active = 1;
    lp_apply_mode();

    lp_out_str("\x1b[?1049h");
    lp_out_str("\x1b[2J\x1b[H");
    lp_out_str("\x1b[0m\x1b(B");
    if (_cursor_vis == 0) {
        lp_out_str("\x1b[?25l");
        _term_cursor_visible = 0;
    } else {
        lp_out_str("\x1b[?25h");
        _term_cursor_visible = 1;
    }
    if (WIN(stdscr)->use_keypad) lp_out_str("\x1b[?1h\x1b=");
    lp_out_flush();

    _term_attr = 0xffffffffU;
    _term_alt = 0;
    _phys_y = -1;
    _phys_x = -1;
    _pending = 1;
    _clear_pending = 1;
    return OK;
}

int lp_detect_resize(void)
{
    int rows;
    int cols;

    if (!_active || stdscr == NULL || _endwin) return 0;
    if (lp_query_size(&rows, &cols) != OK) return 0;
    if (rows == LINES && cols == COLS) return 0;
    if (lp_resize_screen(rows, cols) != OK) return 0;
    return 1;
}

WINDOW *initscr(void)
{
    int rows = 24;
    int cols = 80;

    if (stdscr != NULL) {
        (void)lp_ensure_active();
        return stdscr;
    }

    if (lp_query_size(&rows, &cols) != OK) {
        rows = 24;
        cols = 80;
    }
    if (rows < 1) rows = 24;
    if (cols < 1) cols = 80;

    stdscr = lp_screen_window_alloc(rows, cols);
    if (stdscr == NULL) return NULL;
    curscr = lp_screen_window_alloc(rows, cols);
    _newscr = lp_screen_window_alloc(rows, cols);
    if (curscr == NULL || _newscr == NULL) {
        if (curscr != NULL) { free(WIN(curscr)->cells); free(WIN(curscr)); free(curscr); }
        if (_newscr != NULL) { free(WIN(_newscr)->cells); free(WIN(_newscr)); free(_newscr); }
        free(WIN(stdscr)->cells);
        free(WIN(stdscr));
        free(stdscr);
        stdscr = NULL;
        curscr = NULL;
        _newscr = NULL;
        return NULL;
    }

    lp_fill_cells(WIN(curscr)->cells, rows * cols, CELL_INVALID, A_NORMAL);
    _newscr_force = (unsigned char *)calloc((size_t)rows * (size_t)cols,
                                            sizeof(unsigned char));
    if (_newscr_force == NULL) {
        free(WIN(stdscr)->cells); free(WIN(stdscr)); free(stdscr);
        free(WIN(curscr)->cells); free(WIN(curscr)); free(curscr);
        free(WIN(_newscr)->cells); free(WIN(_newscr)); free(_newscr);
        stdscr = NULL; curscr = NULL; _newscr = NULL;
        return NULL;
    }

    _winlist = stdscr;
    WIN(stdscr)->next = NULL;
    WIN(stdscr)->root = stdscr;
    WIN(stdscr)->owns = 1;

    LINES = rows;
    COLS = cols;
    COLORS = 256;
    COLOR_PAIRS = 256;

    lp_color_reset();

    _cbreak = 1;
    _raw = 0;
    _echo_mode = 1;
    _cursor_vis = 1;
    _term_cursor_visible = 1;
    _endwin = 0;
    _active = 1;
    _pending = 1;
    _clear_pending = 1;
    _resize_event = 0;
    _last_win = stdscr;

    _have_termios = (tcgetattr(0, &_orig_termios) == 0);
    lp_apply_mode();

    lp_out_str("\x1b[?1049h");
    lp_out_str("\x1b[2J\x1b[H");
    lp_out_str("\x1b[0m\x1b(B");
    lp_out_str("\x1b[?25h");
    lp_out_flush();

    _term_attr = 0xffffffffU;
    _term_alt = 0;
    _phys_y = -1;
    _phys_x = -1;
    return stdscr;
}

int endwin(void)
{
    if (stdscr == NULL) return ERR;

    lp_out_str("\x1b[0m\x1b(B");
    lp_out_str("\x1b[?25h");
    lp_out_str("\x1b[?1l\x1b>");
    lp_out_str("\x1b[?1049l");
    lp_out_flush();

    if (_have_termios) (void)tcsetattr(0, TCSAFLUSH, &_orig_termios);

    _term_attr = 0xffffffffU;
    _term_alt = 0;
    _phys_y = -1;
    _phys_x = -1;
    _term_cursor_visible = 1;
    _pending = 0;
    _clear_pending = 0;
    _endwin = 1;
    _active = 0;
    return OK;
}

bool isendwin(void)
{
    return (bool)(_endwin != 0);
}

int curs_set(int visibility)
{
    int old;

    if (visibility < 0 || visibility > 2) return ERR;
    old = _cursor_vis;
    _cursor_vis = visibility;
    if (stdscr != NULL && !_endwin) {
        if (visibility == 0) {
            lp_out_str("\x1b[?25l");
            _term_cursor_visible = 0;
        } else {
            lp_out_str("\x1b[?25h");
            _term_cursor_visible = 1;
        }
        lp_out_flush();
    }
    return old;
}
