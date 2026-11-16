/*
 * lpcurses boxes, borders and lines.
 */
#include "internal.h"

int wborder(WINDOW *win, chtype ls, chtype rs, chtype ts, chtype bs,
            chtype tl, chtype tr, chtype bl, chtype br)
{
    int alt;
    int oldy;
    int oldx;
    int y;
    int x;

    if (win == NULL) return ERR;
    if (win->_maxy < 1 || win->_maxx < 1) return OK;

    alt = (win->_attrs & A_ALTCHARSET) != 0;
    if (ls == 0) ls = alt ? ACS_VLINE : (chtype)'|';
    if (rs == 0) rs = alt ? ACS_VLINE : (chtype)'|';
    if (ts == 0) ts = alt ? ACS_HLINE : (chtype)'-';
    if (bs == 0) bs = alt ? ACS_HLINE : (chtype)'-';
    if (tl == 0) tl = alt ? ACS_ULCORNER : (chtype)'+';
    if (tr == 0) tr = alt ? ACS_URCORNER : (chtype)'+';
    if (bl == 0) bl = alt ? ACS_LLCORNER : (chtype)'+';
    if (br == 0) br = alt ? ACS_LRCORNER : (chtype)'+';

    oldy = win->_cury;
    oldx = win->_curx;

    for (x = 1; x < win->_maxx - 1; x++) {
        lp_put_chtype_at(win, 0, x, ts);
        lp_put_chtype_at(win, win->_maxy - 1, x, bs);
    }
    for (y = 1; y < win->_maxy - 1; y++) {
        lp_put_chtype_at(win, y, 0, ls);
        lp_put_chtype_at(win, y, win->_maxx - 1, rs);
    }
    if (win->_maxy > 1 && win->_maxx > 1) {
        lp_put_chtype_at(win, 0, 0, tl);
        lp_put_chtype_at(win, 0, win->_maxx - 1, tr);
        lp_put_chtype_at(win, win->_maxy - 1, 0, bl);
        lp_put_chtype_at(win, win->_maxy - 1, win->_maxx - 1, br);
    } else {
        lp_put_chtype_at(win, 0, 0, tl);
        if (win->_maxx > 1) lp_put_chtype_at(win, 0, win->_maxx - 1, tr);
        if (win->_maxy > 1) lp_put_chtype_at(win, win->_maxy - 1, 0, bl);
    }

    win->_cury = oldy;
    win->_curx = oldx;
    return OK;
}

int box(WINDOW *win, chtype verch, chtype horch)
{
    int alt;

    if (win == NULL) return ERR;
    if (verch == 0 && horch == 0)
        return wborder(win, 0, 0, 0, 0, 0, 0, 0, 0);

    alt = (win->_attrs & A_ALTCHARSET) != 0;
    return wborder(win,
                   verch, verch, horch, horch,
                   alt ? ACS_ULCORNER : (chtype)'+',
                   alt ? ACS_URCORNER : (chtype)'+',
                   alt ? ACS_LLCORNER : (chtype)'+',
                   alt ? ACS_LRCORNER : (chtype)'+');
}

int border(WINDOW *win, chtype ls, chtype rs, chtype ts, chtype bs,
           chtype tl, chtype tr, chtype bl, chtype br)
{
    return wborder(win, ls, rs, ts, bs, tl, tr, bl, br);
}

int whline(WINDOW *win, chtype ch, int n)
{
    int oldy;
    int oldx;
    int i;

    if (win == NULL || n <= 0) return ERR;
    if (ch == 0)
        ch = (win->_attrs & A_ALTCHARSET) ? ACS_HLINE : (chtype)'-';
    oldy = win->_cury;
    oldx = win->_curx;
    for (i = 0; i < n && oldx + i < win->_maxx; i++)
        lp_put_chtype_at(win, oldy, oldx + i, ch);
    win->_cury = oldy;
    win->_curx = oldx;
    return OK;
}

int hline(chtype ch, int n)
{
    return whline(stdscr, ch, n);
}

int wvline(WINDOW *win, chtype ch, int n)
{
    int oldy;
    int oldx;
    int i;

    if (win == NULL || n <= 0) return ERR;
    if (ch == 0)
        ch = (win->_attrs & A_ALTCHARSET) ? ACS_VLINE : (chtype)'|';
    oldy = win->_cury;
    oldx = win->_curx;
    for (i = 0; i < n && oldy + i < win->_maxy; i++)
        lp_put_chtype_at(win, oldy + i, oldx, ch);
    win->_cury = oldy;
    win->_curx = oldx;
    return OK;
}

int vline(chtype ch, int n)
{
    return wvline(stdscr, ch, n);
}
