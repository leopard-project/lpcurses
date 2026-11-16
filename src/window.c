/*
 * lpcurses window allocation, geometry and screen-wide buffer resizing.
 */
#include "internal.h"

WINDOW *lp_window_alloc(int nlines, int ncols, int begy, int begx,
                        int own, WINDOW *root, struct lpc_cell *cells)
{
    WINDOW *w = (WINDOW *)calloc(1, sizeof(WINDOW));
    struct lpc_win_internal *wi;

    if (w == NULL) return NULL;
    wi = (struct lpc_win_internal *)calloc(1, sizeof(struct lpc_win_internal));
    if (wi == NULL) {
        free(w);
        return NULL;
    }

    w->_internal = wi;
    w->_maxy = nlines;
    w->_maxx = ncols;
    w->_begy = begy;
    w->_begx = begx;
    w->_attrs = A_NORMAL;
    w->_bkgd = (chtype)' ';
    wi->delay = -1;
    wi->root = root != NULL ? root : w;
    wi->next = _winlist;
    _winlist = w;

    if (cells != NULL) {
        wi->cells = cells;
        wi->owns = 0;
    } else {
        size_t count = (size_t)nlines * (size_t)ncols;
        wi->cells = (struct lpc_cell *)malloc(count * sizeof(struct lpc_cell));
        if (wi->cells == NULL) {
            _winlist = wi->next;
            free(wi);
            free(w);
            return NULL;
        }
        lp_fill_cells(wi->cells, (int)count, ' ', A_NORMAL);
        wi->owns = own;
    }
    return w;
}

WINDOW *lp_screen_window_alloc(int rows, int cols)
{
    WINDOW *w = (WINDOW *)calloc(1, sizeof(WINDOW));
    struct lpc_win_internal *wi;
    size_t count;

    if (w == NULL) return NULL;
    wi = (struct lpc_win_internal *)calloc(1, sizeof(struct lpc_win_internal));
    if (wi == NULL) {
        free(w);
        return NULL;
    }

    count = (size_t)rows * (size_t)cols;
    wi->cells = (struct lpc_cell *)malloc(count * sizeof(struct lpc_cell));
    if (wi->cells == NULL) {
        free(wi);
        free(w);
        return NULL;
    }
    lp_fill_cells(wi->cells, (int)count, ' ', A_NORMAL);

    w->_internal = wi;
    w->_maxy = rows;
    w->_maxx = cols;
    w->_attrs = A_NORMAL;
    w->_bkgd = (chtype)' ';
    wi->delay = -1;
    wi->owns = 1;
    wi->root = w;
    return w;
}

void lp_window_unlink(WINDOW *w)
{
    WINDOW **pp = &_winlist;

    while (*pp != NULL) {
        if (*pp == w) {
            *pp = WIN(w)->next;
            return;
        }
        pp = &WIN(*pp)->next;
    }
}

int lp_resize_screen(int rows, int cols)
{
    struct lpc_cell *old_std;
    struct lpc_cell *old_cur;
    struct lpc_cell *old_new;
    unsigned char *old_force;
    struct lpc_cell *new_std;
    struct lpc_cell *new_cur;
    struct lpc_cell *new_new;
    unsigned char *new_force;
    int old_rows;
    int old_cols;
    int copy_rows;
    int copy_cols;
    int y;
    size_t count;

    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;
    if (rows == LINES && cols == COLS) return OK;

    count = (size_t)rows * (size_t)cols;
    new_std = (struct lpc_cell *)malloc(count * sizeof(struct lpc_cell));
    new_cur = (struct lpc_cell *)malloc(count * sizeof(struct lpc_cell));
    new_new = (struct lpc_cell *)malloc(count * sizeof(struct lpc_cell));
    new_force = (unsigned char *)calloc(count, sizeof(unsigned char));
    if (new_std == NULL || new_cur == NULL || new_new == NULL || new_force == NULL) {
        free(new_std);
        free(new_cur);
        free(new_new);
        free(new_force);
        return ERR;
    }

    lp_fill_cells(new_std, (int)count, ' ', A_NORMAL);
    lp_fill_cells(new_cur, (int)count, CELL_INVALID, A_NORMAL);
    lp_fill_cells(new_new, (int)count, ' ', A_NORMAL);

    old_std = WIN(stdscr)->cells;
    old_cur = WIN(curscr)->cells;
    old_new = WIN(_newscr)->cells;
    old_force = _newscr_force;
    old_rows = LINES;
    old_cols = COLS;

    copy_rows = old_rows < rows ? old_rows : rows;
    copy_cols = old_cols < cols ? old_cols : cols;
    for (y = 0; y < copy_rows; y++) {
        memcpy(new_std + (size_t)y * (size_t)cols,
               old_std + (size_t)y * (size_t)old_cols,
               (size_t)copy_cols * sizeof(struct lpc_cell));
    }

    WIN(stdscr)->cells = new_std;
    stdscr->_maxy = rows;
    stdscr->_maxx = cols;
    stdscr->_begy = 0;
    stdscr->_begx = 0;
    if (stdscr->_cury >= rows) stdscr->_cury = rows - 1;
    if (stdscr->_curx >= cols) stdscr->_curx = cols - 1;
    if (stdscr->_maxy < 1) stdscr->_maxy = 1;
    if (stdscr->_maxx < 1) stdscr->_maxx = 1;

    /* Every window that shares stdscr's buffer must be re-based. */
    {
        WINDOW *w;
        for (w = _winlist; w != NULL; w = WIN(w)->next) {
            int ry;
            int rx;

            if (w == stdscr || WIN(w)->root != stdscr) continue;
            ry = w->_begy;
            rx = w->_begx;
            if (ry < 0) ry = 0;
            if (rx < 0) rx = 0;
            if (ry >= rows) ry = rows - 1;
            if (rx >= cols) rx = cols - 1;
            if (w->_maxy > rows - ry) w->_maxy = rows - ry;
            if (w->_maxx > cols - rx) w->_maxx = cols - rx;
            if (w->_maxy < 1) w->_maxy = 1;
            if (w->_maxx < 1) w->_maxx = 1;
            w->_begy = ry;
            w->_begx = rx;
            if (w->_cury >= w->_maxy) w->_cury = w->_maxy - 1;
            if (w->_curx >= w->_maxx) w->_curx = w->_maxx - 1;
            WIN(w)->cells = new_std + (size_t)ry * (size_t)cols + (size_t)rx;
        }
    }

    WIN(curscr)->cells = new_cur;
    curscr->_maxy = rows;
    curscr->_maxx = cols;
    WIN(_newscr)->cells = new_new;
    _newscr->_maxy = rows;
    _newscr->_maxx = cols;
    _newscr_force = new_force;

    free(old_std);
    free(old_cur);
    free(old_new);
    free(old_force);

    LINES = rows;
    COLS = cols;
    _pending = 1;
    _clear_pending = 1;
    _term_attr = 0xffffffffU;
    _term_alt = 0;
    _phys_y = -1;
    _phys_x = -1;
    return OK;
}

int wmove(WINDOW *win, int y, int x)
{
    if (win == NULL) return ERR;
    if (y < 0 || x < 0 || y >= win->_maxy || x >= win->_maxx) return ERR;
    win->_cury = y;
    win->_curx = x;
    return OK;
}

int move(int y, int x)
{
    return wmove(stdscr, y, x);
}

int getcury(const WINDOW *win)
{
    return win != NULL ? win->_cury : ERR;
}

int getcurx(const WINDOW *win)
{
    return win != NULL ? win->_curx : ERR;
}

int getbegy(const WINDOW *win)
{
    return win != NULL ? win->_begy : ERR;
}

int getbegx(const WINDOW *win)
{
    return win != NULL ? win->_begx : ERR;
}

int getmaxy(const WINDOW *win)
{
    return win != NULL ? win->_maxy : ERR;
}

int getmaxx(const WINDOW *win)
{
    return win != NULL ? win->_maxx : ERR;
}

WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x)
{
    if (stdscr == NULL) return NULL;
    if (begin_y < 0 || begin_x < 0) return NULL;
    if (begin_y >= LINES || begin_x >= COLS) return NULL;

    if (nlines <= 0) nlines = LINES - begin_y;
    if (ncols <= 0) ncols = COLS - begin_x;
    if (nlines <= 0 || ncols <= 0) return NULL;
    if (begin_y + nlines > LINES) nlines = LINES - begin_y;
    if (begin_x + ncols > COLS) ncols = COLS - begin_x;

    return lp_window_alloc(nlines, ncols, begin_y, begin_x, 1, NULL, NULL);
}

WINDOW *subwin(WINDOW *orig, int nlines, int ncols, int begin_y, int begin_x)
{
    WINDOW *root;
    int ry;
    int rx;
    struct lpc_cell *base;

    if (orig == NULL) return NULL;
    root = WIN(orig)->root;
    if (root == NULL) return NULL;

    ry = begin_y - root->_begy;
    rx = begin_x - root->_begx;
    if (ry < 0 || rx < 0) return NULL;

    if (nlines <= 0) nlines = orig->_maxy - (begin_y - orig->_begy);
    if (ncols <= 0) ncols = orig->_maxx - (begin_x - orig->_begx);
    if (nlines <= 0 || ncols <= 0) return NULL;

    /* A subwindow must fit inside the root buffer. */
    if (begin_y + nlines > root->_begy + root->_maxy ||
        begin_x + ncols > root->_begx + root->_maxx)
        return NULL;

    base = WIN(root)->cells + (size_t)ry * (size_t)root->_maxx + (size_t)rx;
    {
        WINDOW *w = lp_window_alloc(nlines, ncols, begin_y, begin_x, 0, root, base);
        if (w != NULL) WIN(w)->parent = orig;
        return w;
    }
}

WINDOW *derwin(WINDOW *orig, int nlines, int ncols, int begin_y, int begin_x)
{
    if (orig == NULL) return NULL;
    return subwin(orig, nlines, ncols,
                  orig->_begy + begin_y, orig->_begx + begin_x);
}

int delwin(WINDOW *win)
{
    if (win == NULL) return ERR;
    if (win == stdscr || win == curscr || win == _newscr) return ERR;

    lp_window_unlink(win);
    if (_last_win == win) _last_win = stdscr;
    if (WIN(win)->owns) free(WIN(win)->cells);
    free(WIN(win));
    free(win);
    return OK;
}

int werase(WINDOW *win)
{
    int y;

    if (win == NULL) return ERR;
    for (y = 0; y < win->_maxy; y++)
        lp_blank_range(win, y, 0, win->_maxx - 1, lp_blank_attr(win));
    win->_cury = 0;
    win->_curx = 0;
    return OK;
}

int wclear(WINDOW *win)
{
    if (werase(win) == ERR) return ERR;
    WIN(win)->clear = 1;
    return OK;
}

int erase(void)
{
    return werase(stdscr);
}

int clear(void)
{
    return wclear(stdscr);
}

int clearok(WINDOW *win, bool bf)
{
    if (win == NULL) return ERR;
    WIN(win)->clear = bf ? 1 : 0;
    return OK;
}

int touchwin(WINDOW *win)
{
    if (win == NULL) return ERR;
    WIN(win)->force = 1;
    return OK;
}

int redrawwin(WINDOW *win)
{
    return touchwin(win);
}
