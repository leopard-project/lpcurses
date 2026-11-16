/*
 * lpcurses character output, editing operations and attributes.
 */
#include "internal.h"

static int win_newline(WINDOW *w)
{
    w->_curx = 0;
    if (w->_cury + 1 < w->_maxy) {
        w->_cury++;
        return OK;
    }
    if (WIN(w)->scroll) {
        (void)wscrl(w, 1);
        return OK;
    }
    return ERR;
}

static int win_advance(WINDOW *w, int width)
{
    w->_curx += width;
    if (w->_curx < w->_maxx) return OK;
    if (WIN(w)->scroll) {
        w->_curx = 0;
        return win_newline(w);
    }
    w->_curx = w->_maxx - 1;
    return ERR;
}

static int win_add_cp(WINDOW *w, uint32_t cp, attr_t attr)
{
    int width;

    switch (cp) {
    case '\n':
        return win_newline(w);
    case '\r':
        w->_curx = 0;
        return OK;
    case '\b':
        if (w->_curx > 0) w->_curx--;
        return OK;
    case '\t': {
        int target = (w->_curx + 8) & ~7;
        int rc = OK;
        if (target >= w->_maxx) target = w->_maxx;
        while (w->_curx < target) {
            lp_cell_store(w, w->_cury, w->_curx, ' ', attr);
            rc = win_advance(w, 1);
            if (rc == ERR) break;
        }
        return rc;
    }
    default:
        break;
    }

    if (cp < 32U || cp == 127U) {
        uint32_t visible = (cp == 127U) ? '?' : (uint32_t)(cp + 64U);
        int rc = win_add_cp(w, '^', attr);
        if (rc == ERR) return ERR;
        return win_add_cp(w, visible, attr);
    }

    width = lp_cp_width(cp);
    if (width <= 0) return OK;
    lp_cell_store(w, w->_cury, w->_curx, cp, attr);
    return win_advance(w, width);
}

int waddch(WINDOW *win, chtype ch)
{
    uint32_t raw;
    uint32_t cp;
    attr_t attr;

    if (win == NULL) return ERR;
    raw = (uint32_t)ch;
    attr = win->_attrs;

    if (raw & A_ATTRIBUTES) {
        uint32_t color = raw & A_COLOR;
        if (color != 0) attr = (attr & ~A_COLOR) | color;
        attr |= (raw & A_ATTRIBUTES & ~A_COLOR);
        cp = raw & A_CHARTEXT;
    } else {
        cp = raw;
    }

    if (cp == 0) cp = ' ';
    return win_add_cp(win, cp, attr);
}

int waddnstr(WINDOW *win, const char *str, int n)
{
    int i = 0;
    int rc = OK;

    if (win == NULL || str == NULL) return ERR;

    while (str[i] != '\0' && (n < 0 || i < n)) {
        uint32_t cp;
        int used = lp_utf8_decode((const unsigned char *)str + i,
                                  n < 0 ? 4 : n - i, &cp);
        if (used <= 0) break;
        if (win_add_cp(win, cp, win->_attrs) == ERR) rc = ERR;
        i += used;
    }
    return rc;
}

int waddstr(WINDOW *win, const char *str)
{
    return waddnstr(win, str, -1);
}

int addch(chtype ch)
{
    return waddch(stdscr, ch);
}

int addstr(const char *str)
{
    return waddstr(stdscr, str);
}

int addnstr(const char *str, int n)
{
    return waddnstr(stdscr, str, n);
}

int mvaddch(int y, int x, chtype ch)
{
    if (move(y, x) == ERR) return ERR;
    return waddch(stdscr, ch);
}

int mvwaddch(WINDOW *win, int y, int x, chtype ch)
{
    if (wmove(win, y, x) == ERR) return ERR;
    return waddch(win, ch);
}

int mvaddstr(int y, int x, const char *str)
{
    if (move(y, x) == ERR) return ERR;
    return waddstr(stdscr, str);
}

int mvwaddstr(WINDOW *win, int y, int x, const char *str)
{
    if (wmove(win, y, x) == ERR) return ERR;
    return waddstr(win, str);
}

int mvaddnstr(int y, int x, const char *str, int n)
{
    if (move(y, x) == ERR) return ERR;
    return waddnstr(stdscr, str, n);
}

int mvwaddnstr(WINDOW *win, int y, int x, const char *str, int n)
{
    if (wmove(win, y, x) == ERR) return ERR;
    return waddnstr(win, str, n);
}

static int printw_va(WINDOW *win, const char *fmt, va_list ap)
{
    char stackbuf[512];
    va_list copy;
    int needed;
    int rc;

    if (win == NULL || fmt == NULL) return ERR;

    va_copy(copy, ap);
    needed = vsnprintf(stackbuf, sizeof(stackbuf), fmt, copy);
    va_end(copy);
    if (needed < 0) return ERR;

    if ((size_t)needed < sizeof(stackbuf))
        return waddnstr(win, stackbuf, needed);

    {
        char *heap = (char *)malloc((size_t)needed + 1);
        if (heap == NULL) return ERR;
        va_copy(copy, ap);
        (void)vsnprintf(heap, (size_t)needed + 1, fmt, copy);
        va_end(copy);
        rc = waddnstr(win, heap, needed);
        free(heap);
    }
    return rc;
}

int wprintw(WINDOW *win, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = printw_va(win, fmt, ap);
    va_end(ap);
    return rc;
}

int printw(const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = printw_va(stdscr, fmt, ap);
    va_end(ap);
    return rc;
}

int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...)
{
    va_list ap;
    int rc;

    if (wmove(win, y, x) == ERR) return ERR;
    va_start(ap, fmt);
    rc = printw_va(win, fmt, ap);
    va_end(ap);
    return rc;
}

int mvprintw(int y, int x, const char *fmt, ...)
{
    va_list ap;
    int rc;

    if (move(y, x) == ERR) return ERR;
    va_start(ap, fmt);
    rc = printw_va(stdscr, fmt, ap);
    va_end(ap);
    return rc;
}

int wechochar(WINDOW *win, chtype ch)
{
    if (waddch(win, ch) == ERR) return ERR;
    return wrefresh(win);
}

int echochar(chtype ch)
{
    return wechochar(stdscr, ch);
}

static void decode_chtype(WINDOW *w, chtype ch, uint32_t *cp, attr_t *attr)
{
    uint32_t raw = (uint32_t)ch;
    attr_t a = w->_attrs;

    if (raw & A_ATTRIBUTES) {
        uint32_t color = raw & A_COLOR;
        if (color != 0) a = (a & ~A_COLOR) | color;
        a |= (raw & A_ATTRIBUTES & ~A_COLOR);
        *cp = raw & A_CHARTEXT;
    } else {
        *cp = raw;
    }
    if (*cp == 0) *cp = ' ';
    *attr = a;
}

void lp_put_chtype_at(WINDOW *w, int y, int x, chtype ch)
{
    uint32_t cp;
    attr_t attr;

    decode_chtype(w, ch, &cp, &attr);
    lp_cell_store(w, y, x, cp, attr);
}

int winsch(WINDOW *win, chtype ch)
{
    struct lpc_cell *row;
    uint32_t cp;
    attr_t attr;
    int x;

    if (win == NULL) return ERR;
    if (win->_curx >= win->_maxx || win->_cury >= win->_maxy) return ERR;

    row = WIN(win)->cells + (size_t)win->_cury * (size_t)win->_maxx;
    x = win->_curx;
    if (x + 1 < win->_maxx)
        memmove(&row[x + 1], &row[x],
                (size_t)(win->_maxx - x - 1) * sizeof(struct lpc_cell));

    decode_chtype(win, ch, &cp, &attr);
    row[x].ch = cp;
    row[x].attr = attr;
    if (lp_cp_width(cp) == 2 && x + 1 < win->_maxx) {
        row[x + 1].ch = 0;
        row[x + 1].attr = attr;
    }
    return OK;
}

int insch(chtype ch)
{
    return winsch(stdscr, ch);
}

int wdelch(WINDOW *win)
{
    struct lpc_cell *row;
    int x;

    if (win == NULL) return ERR;
    if (win->_curx >= win->_maxx || win->_cury >= win->_maxy) return ERR;

    row = WIN(win)->cells + (size_t)win->_cury * (size_t)win->_maxx;
    x = win->_curx;
    if (x + 1 < win->_maxx)
        memmove(&row[x], &row[x + 1],
                (size_t)(win->_maxx - x - 1) * sizeof(struct lpc_cell));
    row[win->_maxx - 1].ch = ' ';
    row[win->_maxx - 1].attr = lp_blank_attr(win);
    return OK;
}

int delch(void)
{
    return wdelch(stdscr);
}

int wclrtoeol(WINDOW *win)
{
    if (win == NULL) return ERR;
    lp_blank_range(win, win->_cury, win->_curx, win->_maxx - 1,
                   lp_blank_attr(win));
    return OK;
}

int clrtoeol(void)
{
    return wclrtoeol(stdscr);
}

int wclrtobot(WINDOW *win)
{
    int y;

    if (win == NULL) return ERR;
    lp_blank_range(win, win->_cury, win->_curx, win->_maxx - 1,
                   lp_blank_attr(win));
    for (y = win->_cury + 1; y < win->_maxy; y++)
        lp_blank_range(win, y, 0, win->_maxx - 1, lp_blank_attr(win));
    return OK;
}

int clrtobot(void)
{
    return wclrtobot(stdscr);
}

int winsertln(WINDOW *win)
{
    struct lpc_cell *row;
    int count;

    if (win == NULL) return ERR;
    if (win->_cury >= win->_maxy) return ERR;

    row = WIN(win)->cells + (size_t)win->_cury * (size_t)win->_maxx;
    count = win->_maxy - win->_cury - 1;
    if (count > 0)
        memmove(row + win->_maxx, row,
                (size_t)count * (size_t)win->_maxx * sizeof(struct lpc_cell));
    lp_blank_range(win, win->_cury, 0, win->_maxx - 1, lp_blank_attr(win));
    return OK;
}

int insertln(void)
{
    return winsertln(stdscr);
}

int wdeleteln(WINDOW *win)
{
    struct lpc_cell *row;
    int count;

    if (win == NULL) return ERR;
    if (win->_cury >= win->_maxy) return ERR;

    row = WIN(win)->cells + (size_t)win->_cury * (size_t)win->_maxx;
    count = win->_maxy - win->_cury - 1;
    if (count > 0)
        memmove(row, row + win->_maxx,
                (size_t)count * (size_t)win->_maxx * sizeof(struct lpc_cell));
    lp_blank_range(win, win->_maxy - 1, 0, win->_maxx - 1,
                   lp_blank_attr(win));
    return OK;
}

int deleteln(void)
{
    return wdeleteln(stdscr);
}

int scrollok(WINDOW *win, bool bf)
{
    if (win == NULL) return ERR;
    WIN(win)->scroll = bf ? 1 : 0;
    return OK;
}

int idlok(WINDOW *win, bool bf)
{
    if (win == NULL) return ERR;
    (void)bf;
    return OK;
}

int wscrl(WINDOW *win, int n)
{
    struct lpc_cell *cells;
    int rows;
    int cols;
    int y;

    if (win == NULL) return ERR;
    if (n == 0) return OK;
    if (!WIN(win)->scroll) return ERR;

    cells = WIN(win)->cells;
    rows = win->_maxy;
    cols = win->_maxx;

    if (n > 0) {
        if (n >= rows) {
            lp_fill_cells(cells, rows * cols, ' ', lp_blank_attr(win));
            return OK;
        }
        memmove(cells, cells + (size_t)n * (size_t)cols,
                (size_t)(rows - n) * (size_t)cols * sizeof(struct lpc_cell));
        for (y = rows - n; y < rows; y++)
            lp_blank_range(win, y, 0, cols - 1, lp_blank_attr(win));
    } else {
        int amount = -n;
        if (amount >= rows) {
            lp_fill_cells(cells, rows * cols, ' ', lp_blank_attr(win));
            return OK;
        }
        memmove(cells + (size_t)amount * (size_t)cols, cells,
                (size_t)(rows - amount) * (size_t)cols * sizeof(struct lpc_cell));
        for (y = 0; y < amount; y++)
            lp_blank_range(win, y, 0, cols - 1, lp_blank_attr(win));
    }
    return OK;
}

int scroll(WINDOW *win)
{
    return wscrl(win, 1);
}

int wattron(WINDOW *win, chtype attrs)
{
    if (win == NULL) return ERR;
    win->_attrs |= (attr_t)attrs;
    return OK;
}

int wattroff(WINDOW *win, chtype attrs)
{
    if (win == NULL) return ERR;
    win->_attrs &= ~(attr_t)attrs;
    return OK;
}

int wattrset(WINDOW *win, chtype attrs)
{
    int old;

    if (win == NULL) return ERR;
    old = (int)win->_attrs;
    win->_attrs = (attr_t)attrs;
    return old;
}

int attron(chtype attrs)
{
    return wattron(stdscr, attrs);
}

int attroff(chtype attrs)
{
    return wattroff(stdscr, attrs);
}

int attrset(chtype attrs)
{
    return wattrset(stdscr, attrs);
}

int standout(void)
{
    return wattron(stdscr, A_STANDOUT);
}

int standend(void)
{
    return wattrset(stdscr, A_NORMAL);
}
