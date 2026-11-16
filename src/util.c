/*
 * lpcurses character, UTF-8, timing and cell-buffer helpers.
 */
#include "internal.h"

int lp_isspace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

int lp_isdigit(int c)
{
    return c >= '0' && c <= '9';
}

int lp_digit_value(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

long long lp_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000L);
}

int lp_cp_width(uint32_t cp)
{
    if (cp == 0) return 0;

    if ((cp >= 0x0300 && cp <= 0x036f) ||
        (cp >= 0x1ab0 && cp <= 0x1aff) ||
        (cp >= 0x1dc0 && cp <= 0x1dff) ||
        (cp >= 0x20d0 && cp <= 0x20ff) ||
        (cp >= 0xfe20 && cp <= 0xfe2f))
        return 0;

    if ((cp >= 0x1100 && cp <= 0x115f) ||
        (cp >= 0x2e80 && cp <= 0xa4cf) ||
        (cp >= 0xac00 && cp <= 0xd7a3) ||
        (cp >= 0xf900 && cp <= 0xfaff) ||
        (cp >= 0xfe10 && cp <= 0xfe19) ||
        (cp >= 0xfe30 && cp <= 0xfe6f) ||
        (cp >= 0xff00 && cp <= 0xff60) ||
        (cp >= 0xffe0 && cp <= 0xffe6) ||
        (cp >= 0x1f300 && cp <= 0x1f64f) ||
        (cp >= 0x1f900 && cp <= 0x1f9ff) ||
        (cp >= 0x20000 && cp <= 0x3fffd))
        return 2;

    return 1;
}

int lp_utf8_encode(uint32_t cp, char out[4])
{
    if (cp > 0x10ffffU || (cp >= 0xd800U && cp <= 0xdfffU))
        cp = 0xfffdU;

    if (cp < 0x80U) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800U) {
        out[0] = (char)(0xc0U | (cp >> 6));
        out[1] = (char)(0x80U | (cp & 0x3fU));
        return 2;
    }
    if (cp < 0x10000U) {
        out[0] = (char)(0xe0U | (cp >> 12));
        out[1] = (char)(0x80U | ((cp >> 6) & 0x3fU));
        out[2] = (char)(0x80U | (cp & 0x3fU));
        return 3;
    }
    out[0] = (char)(0xf0U | (cp >> 18));
    out[1] = (char)(0x80U | ((cp >> 12) & 0x3fU));
    out[2] = (char)(0x80U | ((cp >> 6) & 0x3fU));
    out[3] = (char)(0x80U | (cp & 0x3fU));
    return 4;
}

/* Decode one UTF-8 sequence; invalid input consumes one byte. */
int lp_utf8_decode(const unsigned char *s, int n, uint32_t *cp)
{
    unsigned int c;
    int need;
    uint32_t v;
    int i;

    if (n <= 0) return 0;
    c = s[0];
    if (c < 0x80U) {
        *cp = c;
        return 1;
    }
    if (c >= 0xc2U && c <= 0xdfU) {
        need = 1;
        v = c & 0x1fU;
    } else if (c >= 0xe0U && c <= 0xefU) {
        need = 2;
        v = c & 0x0fU;
    } else if (c >= 0xf0U && c <= 0xf4U) {
        need = 3;
        v = c & 0x07U;
    } else {
        *cp = c;
        return 1;
    }

    if (n < need + 1) {
        *cp = c;
        return 1;
    }

    for (i = 1; i <= need; i++) {
        if (s[i] == 0 || (s[i] & 0xc0U) != 0x80U) {
            *cp = c;
            return 1;
        }
        v = (v << 6) | (s[i] & 0x3fU);
    }

    if ((need == 2 && v < 0x800U) ||
        (need == 3 && v < 0x10000U) ||
        (v >= 0xd800U && v <= 0xdfffU) || v > 0x10ffffU) {
        *cp = c;
        return 1;
    }

    *cp = v;
    return need + 1;
}

void lp_fill_cells(struct lpc_cell *cells, int count, uint32_t ch, attr_t attr)
{
    int i;
    for (i = 0; i < count; i++) {
        cells[i].ch = ch;
        cells[i].attr = attr;
    }
}

void lp_blank_range(WINDOW *w, int y, int x0, int x1, attr_t attr)
{
    struct lpc_cell *row;
    int x;

    if (y < 0 || y >= w->_maxy) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= w->_maxx) x1 = w->_maxx - 1;
    if (x0 > x1) return;

    row = WIN(w)->cells + (size_t)y * (size_t)w->_maxx;

    /* Do not leave half of a wide character behind. */
    if (x0 > 0 && row[x0].ch == 0) x0--;
    if (x1 + 1 < w->_maxx && row[x1 + 1].ch == 0) x1++;

    for (x = x0; x <= x1; x++) {
        row[x].ch = ' ';
        row[x].attr = attr;
    }
}

void lp_cell_store(WINDOW *w, int y, int x, uint32_t cp, attr_t attr)
{
    struct lpc_cell *row;
    struct lpc_cell *c;
    int width;

    if (y < 0 || y >= w->_maxy || x < 0 || x >= w->_maxx) return;
    width = lp_cp_width(cp);

    /* Combining marks cannot be represented in this one-scalar cell. */
    if (width == 0) return;

    row = WIN(w)->cells + (size_t)y * (size_t)w->_maxx;
    c = &row[x];

    /* Overwriting the tail of a wide character clears its head. */
    if (c->ch == 0 && x > 0) {
        row[x - 1].ch = ' ';
        row[x - 1].attr = attr;
    }
    /* Overwriting the head clears the old tail. */
    if (x + 1 < w->_maxx && row[x + 1].ch == 0) {
        row[x + 1].ch = ' ';
        row[x + 1].attr = attr;
    }

    if (width == 2 && x + 1 >= w->_maxx) {
        c->ch = ' ';
        c->attr = attr;
        return;
    }

    c->ch = cp;
    c->attr = attr;
    if (width == 2) {
        row[x + 1].ch = 0;
        row[x + 1].attr = attr;
    }
}
