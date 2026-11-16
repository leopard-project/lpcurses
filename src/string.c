/*
 * lpcurses line input and the small scanf implementation used by scanw().
 */
#include "internal.h"

static int text_cells(const char *s)
{
    int i = 0;
    int cells = 0;

    while (s[i] != '\0') {
        uint32_t cp;
        int used = lp_utf8_decode((const unsigned char *)s + i, 4, &cp);
        int w;
        if (used <= 0) break;
        w = lp_cp_width(cp);
        if (w < 0) w = 1;
        cells += w;
        i += used;
    }
    return cells;
}

static void redraw_input(WINDOW *win, int sy, int sx, const char *buf, int *prev)
{
    int neww = text_cells(buf);
    int clear = neww > *prev ? neww : *prev;
    int i;

    (void)wmove(win, sy, sx);
    for (i = 0; i < clear; i++) (void)waddch(win, (chtype)' ');
    (void)wmove(win, sy, sx);
    (void)waddstr(win, buf);
    (void)wrefresh(win);
    *prev = neww;
}

int wgetnstr(WINDOW *win, char *str, int n)
{
    int sy;
    int sx;
    int pos = 0;
    int prev_cells = 0;
    int saved_echo;

    if (win == NULL || str == NULL || n <= 0) return ERR;

    sy = win->_cury;
    sx = win->_curx;
    str[0] = '\0';
    saved_echo = _echo_mode;
    _echo_mode = 0;

    for (;;) {
        int ch = wgetch(win);

        if (ch == ERR || ch == KEY_RESIZE) {
            str[pos] = '\0';
            _echo_mode = saved_echo;
            return ERR;
        }
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) break;
        if (ch == KEY_BACKSPACE || ch == 8 || ch == 127) {
            if (pos > 0) {
                do {
                    pos--;
                } while (pos > 0 &&
                         ((unsigned char)str[pos] & 0xc0U) == 0x80U);
                str[pos] = '\0';
                redraw_input(win, sy, sx, str, &prev_cells);
            }
            continue;
        }
        if (ch == 21) { /* Ctrl-U */
            pos = 0;
            str[0] = '\0';
            redraw_input(win, sy, sx, str, &prev_cells);
            continue;
        }
        if (ch < 32 || ch == 127 || ch >= KEY_MIN) continue;

        if (pos < n - 1) {
            str[pos++] = (char)ch;
            str[pos] = '\0';
            redraw_input(win, sy, sx, str, &prev_cells);
        }
    }

    str[pos] = '\0';
    _echo_mode = saved_echo;
    return OK;
}

int getnstr(char *str, int n)
{
    return wgetnstr(stdscr, str, n);
}

static const char *scan_skip_ws(const char *s)
{
    while (lp_isspace((unsigned char)*s)) s++;
    return s;
}

static double scan_double(const char **sp, int width)
{
    const char *s = *sp;
    int used = 0;
    int neg = 0;
    int digits = 0;
    double value = 0.0;

    if (width > 0 && used >= width) return 0.0;
    if (*s == '+' || *s == '-') {
        neg = (*s == '-');
        s++;
        used++;
    }
    while ((width <= 0 || used < width) && lp_isdigit((unsigned char)*s)) {
        value = value * 10.0 + (double)(*s - '0');
        s++;
        used++;
        digits++;
    }
    if ((width <= 0 || used < width) && *s == '.') {
        double scale = 0.1;
        s++;
        used++;
        while ((width <= 0 || used < width) && lp_isdigit((unsigned char)*s)) {
            value += (double)(*s - '0') * scale;
            scale *= 0.1;
            s++;
            used++;
            digits++;
        }
    }
    if (digits == 0) return 0.0;

    if ((width <= 0 || used < width) && (*s == 'e' || *s == 'E')) {
        const char *save = s;
        int exp_neg = 0;
        int exp = 0;
        int exp_digits = 0;

        s++;
        used++;
        if ((width <= 0 || used < width) && (*s == '+' || *s == '-')) {
            exp_neg = (*s == '-');
            s++;
            used++;
        }
        while ((width <= 0 || used < width) && lp_isdigit((unsigned char)*s)) {
            if (exp < 10000) exp = exp * 10 + (*s - '0');
            s++;
            used++;
            exp_digits++;
        }
        if (exp_digits == 0) {
            s = save;
        } else {
            double factor = 1.0;
            int i;
            for (i = 0; i < exp; i++) factor *= 10.0;
            if (exp_neg) value /= factor;
            else value *= factor;
        }
    }

    *sp = s;
    return neg ? -value : value;
}

static void store_signed(va_list ap, char len, long long v)
{
    switch (len) {
    case 'H': *va_arg(ap, signed char *) = (signed char)v; break;
    case 'h': *va_arg(ap, short *) = (short)v; break;
    case 'l': *va_arg(ap, long *) = (long)v; break;
    case 'L': *va_arg(ap, long long *) = v; break;
    default:  *va_arg(ap, int *) = (int)v; break;
    }
}

static void store_unsigned(va_list ap, char len, unsigned long long v)
{
    switch (len) {
    case 'H': *va_arg(ap, unsigned char *) = (unsigned char)v; break;
    case 'h': *va_arg(ap, unsigned short *) = (unsigned short)v; break;
    case 'l': *va_arg(ap, unsigned long *) = (unsigned long)v; break;
    case 'L': *va_arg(ap, unsigned long long *) = v; break;
    case 'z': *va_arg(ap, size_t *) = (size_t)v; break;
    default:  *va_arg(ap, unsigned int *) = (unsigned int)v; break;
    }
}

static int lp_vsscanf(const char *input, const char *fmt, va_list ap)
{
    const char *s = input;
    int assigned = 0;

    while (*fmt != '\0') {
        int suppress;
        int width;
        char len;
        char conv;

        if (lp_isspace((unsigned char)*fmt)) {
            while (lp_isspace((unsigned char)*fmt)) fmt++;
            s = scan_skip_ws(s);
            continue;
        }
        if (*fmt != '%') {
            if (*s != *fmt) break;
            s++;
            fmt++;
            continue;
        }

        fmt++;
        if (*fmt == '%') {
            if (*s != '%') break;
            s++;
            fmt++;
            continue;
        }

        suppress = 0;
        if (*fmt == '*') {
            suppress = 1;
            fmt++;
        }

        width = 0;
        while (lp_isdigit((unsigned char)*fmt)) {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        len = 0;
        if (*fmt == 'h') {
            fmt++;
            len = 'h';
            if (*fmt == 'h') { len = 'H'; fmt++; }
        } else if (*fmt == 'l') {
            fmt++;
            len = 'l';
            if (*fmt == 'l') { len = 'L'; fmt++; }
        } else if (*fmt == 'z' || *fmt == 'j' || *fmt == 't' || *fmt == 'L') {
            len = *fmt++;
        }

        conv = *fmt++;
        if (conv == '\0') break;

        if (conv != 'c' && conv != '[')
            s = scan_skip_ws(s);

        if (*s == '\0') {
            if (assigned == 0) return -1;
            break;
        }

        if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'o' ||
            conv == 'x' || conv == 'X' || conv == 'p') {
            const char *start = s;
            int neg = 0;
            int base;
            int used = 0;
            int digits = 0;
            int limit = width > 0 ? width : 1000000;
            unsigned long long value = 0;

            if (conv == 'd' || conv == 'u') base = 10;
            else if (conv == 'o') base = 8;
            else if (conv == 'x' || conv == 'X' || conv == 'p') base = 16;
            else base = 0;

            if ((conv == 'd' || conv == 'i' || conv == 'u') &&
                (*s == '+' || *s == '-')) {
                neg = (*s == '-');
                s++;
                used++;
            }

            if (base == 0) {
                if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
                    base = 16;
                    s += 2;
                    used += 2;
                } else if (s[0] == '0') {
                    base = 8;
                } else {
                    base = 10;
                }
            } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
                s += 2;
                used += 2;
            }

            while (*s != '\0' && used < limit) {
                int d = lp_digit_value((unsigned char)*s);
                if (d < 0 || d >= base) break;
                if (value < 0xffffffffffffffffULL / 16ULL)
                    value = value * (unsigned long long)base + (unsigned long long)d;
                s++;
                used++;
                digits++;
            }

            if (digits == 0) {
                s = start;
                break;
            }

            if (neg) value = (unsigned long long)(-(long long)value);

            if (!suppress) {
                if (conv == 'p')
                    *va_arg(ap, void **) = (void *)(uintptr_t)value;
                else if (conv == 'd' || conv == 'i')
                    store_signed(ap, len, (long long)value);
                else
                    store_unsigned(ap, len, value);
                assigned++;
            }
        } else if (conv == 's') {
            char *out = suppress ? NULL : va_arg(ap, char *);
            int limit = width > 0 ? width : 1000000;
            int count = 0;

            while (*s != '\0' && !lp_isspace((unsigned char)*s) && count < limit) {
                if (out != NULL) out[count] = *s;
                s++;
                count++;
            }
            if (count == 0) break;
            if (out != NULL) out[count] = '\0';
            if (!suppress) assigned++;
        } else if (conv == 'c') {
            char *out = suppress ? NULL : va_arg(ap, char *);
            int limit = width > 0 ? width : 1;
            int count = 0;

            while (*s != '\0' && count < limit) {
                if (out != NULL) out[count] = *s;
                s++;
                count++;
            }
            if (count == 0) break;
            if (!suppress) assigned++;
        } else if (conv == '[') {
            unsigned char set[256];
            int negate = 0;
            int limit;
            int count = 0;
            char *out;

            memset(set, 0, sizeof(set));
            if (*fmt == '^') {
                negate = 1;
                fmt++;
            }
            if (*fmt == ']') {
                set[(unsigned char)']'] = 1;
                fmt++;
            }
            while (*fmt != '\0' && *fmt != ']') {
                unsigned char first = (unsigned char)*fmt++;
                if (*fmt == '-' && fmt[1] != '\0' && fmt[1] != ']') {
                    unsigned char last;
                    int c;
                    fmt++;
                    last = (unsigned char)*fmt++;
                    for (c = first; c <= (int)last; c++)
                        set[c] = 1;
                } else {
                    set[first] = 1;
                }
            }
            if (*fmt == ']') fmt++;

            limit = width > 0 ? width : 1000000;
            out = suppress ? NULL : va_arg(ap, char *);
            while (*s != '\0' && count < limit) {
                int in_set = set[(unsigned char)*s] != 0;
                if (negate ? in_set : !in_set) break;
                if (out != NULL) out[count] = *s;
                s++;
                count++;
            }
            if (count == 0) break;
            if (out != NULL) out[count] = '\0';
            if (!suppress) assigned++;
        } else if (conv == 'f' || conv == 'e' || conv == 'g' ||
                   conv == 'E' || conv == 'G') {
            const char *start = s;
            double value = scan_double(&s, width);
            if (s == start) break;
            if (!suppress) {
                if (len == 'l' || len == 'L')
                    *va_arg(ap, double *) = value;
                else
                    *va_arg(ap, float *) = (float)value;
                assigned++;
            }
        } else {
            break;
        }
    }

    return assigned;
}

static int wscanw_va(WINDOW *win, const char *fmt, va_list ap)
{
    char buf[1024];
    int assigned;

    if (win == NULL || fmt == NULL) return ERR;
    if (wgetnstr(win, buf, (int)sizeof(buf)) == ERR) return ERR;
    assigned = lp_vsscanf(buf, fmt, ap);
    if (assigned <= 0) return ERR;
    return assigned;
}

int wscanw(WINDOW *win, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = wscanw_va(win, fmt, ap);
    va_end(ap);
    return rc;
}

int scanw(const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = wscanw_va(stdscr, fmt, ap);
    va_end(ap);
    return rc;
}

int mvwscanw(WINDOW *win, int y, int x, const char *fmt, ...)
{
    va_list ap;
    int rc;

    if (wmove(win, y, x) == ERR) return ERR;
    va_start(ap, fmt);
    rc = wscanw_va(win, fmt, ap);
    va_end(ap);
    return rc;
}

int mvscanw(int y, int x, const char *fmt, ...)
{
    va_list ap;
    int rc;

    if (move(y, x) == ERR) return ERR;
    va_start(ap, fmt);
    rc = wscanw_va(stdscr, fmt, ap);
    va_end(ap);
    return rc;
}
