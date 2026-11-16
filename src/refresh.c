/*
 * lpcurses output buffering, ANSI diff emission and screen refresh.
 */
#include "internal.h"

static char   _obuf[16384];
static size_t _olen = 0;
static int    _out_error = 0;

void lp_out_flush(void)
{
    size_t off = 0;

    while (off < _olen) {
        ssize_t n = write(1, _obuf + off, _olen - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            _out_error = 1;
            break;
        }
        if (n == 0) {
            _out_error = 1;
            break;
        }
        off += (size_t)n;
    }
    _olen = 0;
}

void lp_out_mem(const char *s, size_t n)
{
    while (n > 0) {
        size_t room = sizeof(_obuf) - _olen;
        size_t take;

        if (room == 0) {
            lp_out_flush();
            room = sizeof(_obuf);
        }
        take = n < room ? n : room;
        memcpy(_obuf + _olen, s, take);
        _olen += take;
        s += take;
        n -= take;
    }
}

void lp_out_str(const char *s)
{
    lp_out_mem(s, strlen(s));
}

void lp_out_ch(char c)
{
    lp_out_mem(&c, 1);
}

void lp_out_uint(unsigned int v)
{
    char tmp[12];
    int n = 0;

    if (v == 0) {
        lp_out_ch('0');
        return;
    }
    while (v > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10U));
        v /= 10U;
    }
    while (n > 0) lp_out_ch(tmp[--n]);
}

void lp_out_cp(uint32_t cp)
{
    char tmp[4];
    int n = lp_utf8_encode(cp, tmp);
    lp_out_mem(tmp, (size_t)n);
}

static void emit_cup(int y, int x)
{
    lp_out_str("\x1b[");
    lp_out_uint((unsigned int)(y + 1));
    lp_out_ch(';');
    lp_out_uint((unsigned int)(x + 1));
    lp_out_ch('H');
}

static void emit_color(unsigned int c, int background)
{
    if (c < 8U) {
        lp_out_ch(';');
        lp_out_ch(background ? '4' : '3');
        lp_out_uint(c);
    } else if (c < 16U) {
        if (background) {
            lp_out_str(";10");
        } else {
            lp_out_str(";9");
        }
        lp_out_uint(c - 8U);
    } else {
        lp_out_str(background ? ";48;5;" : ";38;5;");
        lp_out_uint(c);
    }
}

static void emit_sgr(attr_t a)
{
    int pair = PAIR_NUMBER(a);
    short fg = -1;
    short bg = -1;

    lp_out_str("\x1b[0");
    if (a & A_BOLD) lp_out_str(";1");
    if (a & A_DIM) lp_out_str(";2");
    if (a & A_UNDERLINE) lp_out_str(";4");
    if (a & A_BLINK) lp_out_str(";5");
    if (a & (A_REVERSE | A_STANDOUT)) lp_out_str(";7");
    if (a & A_INVIS) lp_out_str(";8");

    lp_color_lookup(pair, &fg, &bg);
    if (fg >= 0 && fg < COLORS) emit_color((unsigned int)fg, 0);
    if (bg >= 0 && bg < COLORS) emit_color((unsigned int)bg, 1);
    lp_out_ch('m');
}

int wnoutrefresh(WINDOW *win)
{
    int y;
    int x;

    if (win == NULL || _newscr == NULL) return ERR;

    if (WIN(win)->clear) _clear_pending = 1;

    for (y = 0; y < win->_maxy; y++) {
        int sy = win->_begy + y;
        if (sy < 0 || sy >= LINES) continue;
        for (x = 0; x < win->_maxx; x++) {
            int sx = win->_begx + x;
            size_t di;
            if (sx < 0 || sx >= COLS) continue;
            di = (size_t)sy * (size_t)COLS + (size_t)sx;
            WIN(_newscr)->cells[di] =
                WIN(win)->cells[(size_t)y * (size_t)win->_maxx + (size_t)x];
            if (WIN(win)->force) _newscr_force[di] = 1;
        }
    }

    WIN(win)->force = 0;
    WIN(win)->clear = 0;
    _last_win = win;
    _pending = 1;
    return OK;
}

int doupdate(void)
{
    int y;
    int x;

    if (stdscr == NULL || _newscr == NULL) return ERR;
    if (lp_ensure_active() != OK) return ERR;

    if (lp_detect_resize()) _resize_event = 1;

    if (!_pending && !_clear_pending) return OK;

    if (_clear_pending) {
        lp_out_str("\x1b[0m");
        lp_out_str("\x1b[2J");
        lp_out_str("\x1b(B");
        lp_fill_cells(WIN(curscr)->cells, LINES * COLS, ' ', A_NORMAL);
        _term_attr = 0xffffffffU;
        _term_alt = 0;
        _phys_y = -1;
        _phys_x = -1;
        _clear_pending = 0;
    }

    for (y = 0; y < LINES; y++) {
        for (x = 0; x < COLS; x++) {
            size_t idx = (size_t)y * (size_t)COLS + (size_t)x;
            struct lpc_cell *want = &WIN(_newscr)->cells[idx];
            struct lpc_cell *phys = &WIN(curscr)->cells[idx];
            int force = (int)_newscr_force[idx];
            int width;
            int alt;

            if (!force && want->ch == phys->ch && want->attr == phys->attr)
                continue;

            _newscr_force[idx] = 0;

            /* Tail of a wide character: the head already painted it. */
            if (want->ch == 0) {
                *phys = *want;
                continue;
            }

            if (_phys_y != y || _phys_x != x) {
                emit_cup(y, x);
                _phys_y = y;
                _phys_x = x;
            }

            if (want->attr != _term_attr) {
                emit_sgr(want->attr);
                _term_attr = want->attr;
            }

            width = lp_cp_width(want->ch);
            if (width <= 0) width = 1;

            alt = (want->ch < 128U && (want->attr & A_ALTCHARSET) != 0);
            if (alt != _term_alt) {
                lp_out_str(alt ? "\x1b(0" : "\x1b(B");
                _term_alt = alt;
            }

            if (alt) lp_out_ch((char)want->ch);
            else lp_out_cp(want->ch);

            *phys = *want;
            _phys_x += width;
            if (_phys_x >= COLS) {
                /* Autowrap makes the physical cursor unpredictable. */
                _phys_y = -1;
                _phys_x = -1;
            }
        }
    }

    if (_cursor_vis == 0) {
        if (_term_cursor_visible) {
            lp_out_str("\x1b[?25l");
            _term_cursor_visible = 0;
        }
    } else {
        WINDOW *cw = _last_win != NULL ? _last_win : stdscr;
        if (!_term_cursor_visible) {
            lp_out_str("\x1b[?25h");
            _term_cursor_visible = 1;
        }
        if (cw != NULL && !WIN(cw)->leave) {
            int cy = cw->_begy + cw->_cury;
            int cx = cw->_begx + cw->_curx;
            if (cy < 0) cy = 0;
            if (cy >= LINES) cy = LINES - 1;
            if (cx < 0) cx = 0;
            if (cx >= COLS) cx = COLS - 1;
            if (cy != _phys_y || cx != _phys_x) {
                emit_cup(cy, cx);
                _phys_y = cy;
                _phys_x = cx;
            }
        }
    }

    lp_out_flush();
    _pending = 0;
    return _out_error ? ERR : OK;
}

int wrefresh(WINDOW *win)
{
    if (wnoutrefresh(win) == ERR) return ERR;
    return doupdate();
}

int refresh(void)
{
    return wrefresh(stdscr);
}
