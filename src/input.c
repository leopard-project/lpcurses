/*
 * lpcurses keyboard modes, ANSI key decoding and timer-based input.
 */
#include "internal.h"

static unsigned char _inbuf[256];
static int _inpos = 0;
static int _inlen = 0;
static unsigned char _bpush[16];
static int _nbpush = 0;
static int _keypush[32];
static int _nkeypush = 0;

int cbreak(void)
{
    _cbreak = 1;
    _raw = 0;
    lp_apply_mode();
    return OK;
}

int nocbreak(void)
{
    _cbreak = 0;
    _raw = 0;
    lp_apply_mode();
    return OK;
}

int raw(void)
{
    _raw = 1;
    _cbreak = 1;
    lp_apply_mode();
    return OK;
}

int noraw(void)
{
    _raw = 0;
    _cbreak = 1;
    lp_apply_mode();
    return OK;
}

int echo(void)
{
    _echo_mode = 1;
    return OK;
}

int noecho(void)
{
    _echo_mode = 0;
    return OK;
}

int keypad(WINDOW *win, bool bf)
{
    if (win == NULL) return ERR;
    WIN(win)->use_keypad = bf ? 1 : 0;
    if (win == stdscr && !_endwin) {
        lp_out_str(bf ? "\x1b[?1h\x1b=" : "\x1b[?1l\x1b>");
        lp_out_flush();
    }
    return OK;
}

int wtimeout(WINDOW *win, int delay)
{
    if (win == NULL) return ERR;
    WIN(win)->delay = delay;
    return OK;
}

int timeout(int delay)
{
    return wtimeout(stdscr, delay);
}

int nodelay(WINDOW *win, bool bf)
{
    return wtimeout(win, bf ? 0 : -1);
}

int halfdelay(int tenths)
{
    if (tenths < 1 || tenths > 255) return ERR;
    (void)cbreak();
    return wtimeout(stdscr, tenths * 100);
}

static int fill_input(int timeout_ms)
{
    struct pollfd pfd;
    int r;

    if (_inpos < _inlen) return 1;
    _inpos = 0;
    _inlen = 0;

    pfd.fd = 0;
    pfd.events = POLLIN;
    pfd.revents = 0;
    r = poll(&pfd, 1, timeout_ms);
    if (r == 0) return 0;
    if (r < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }

    {
        ssize_t n = read(0, _inbuf, sizeof(_inbuf));
        if (n > 0) {
            _inlen = (int)n;
            _inpos = 0;
            return 1;
        }
        if (n < 0 && errno == EINTR) return 0;
        return -1;
    }
}

static int next_byte(int timeout_ms)
{
    if (_nbpush > 0) return (int)_bpush[--_nbpush];
    if (_inpos >= _inlen) {
        int r = fill_input(timeout_ms);
        if (r <= 0) return -1;
    }
    return (int)_inbuf[_inpos++];
}

static void unget_byte(int c)
{
    if (c >= 0 && _nbpush < (int)sizeof(_bpush))
        _bpush[_nbpush++] = (unsigned char)c;
}

static int first_param(const char *p)
{
    int n = 0;
    while (*p >= '0' && *p <= '9') {
        n = n * 10 + (*p - '0');
        p++;
    }
    return n;
}

static int parse_csi(void)
{
    char params[32];
    int n = 0;
    int final = 0;

    for (;;) {
        int c = next_byte(ESCDELAY);
        if (c < 0) return 27;
        if (c >= 0x40 && c <= 0x7e) {
            final = c;
            break;
        }
        if (n < (int)sizeof(params) - 1) params[n++] = (char)c;
    }
    params[n] = '\0';

    switch (final) {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    case 'E': return KEY_B2;
    case 'F': return KEY_END;
    case 'H': return KEY_HOME;
    case 'P': return KEY_F(1);
    case 'Q': return KEY_F(2);
    case 'R': return KEY_F(3);
    case 'S': return KEY_F(4);
    case 'Z': return KEY_BTAB;
    case '~':
        switch (first_param(params)) {
        case 1:  return KEY_HOME;
        case 2:  return KEY_IC;
        case 3:  return KEY_DC;
        case 4:  return KEY_END;
        case 5:  return KEY_PPAGE;
        case 6:  return KEY_NPAGE;
        case 7:  return KEY_HOME;
        case 8:  return KEY_END;
        case 11: return KEY_F(1);
        case 12: return KEY_F(2);
        case 13: return KEY_F(3);
        case 14: return KEY_F(4);
        case 15: return KEY_F(5);
        case 17: return KEY_F(6);
        case 18: return KEY_F(7);
        case 19: return KEY_F(8);
        case 20: return KEY_F(9);
        case 21: return KEY_F(10);
        case 23: return KEY_F(11);
        case 24: return KEY_F(12);
        default: break;
        }
        break;
    default:
        break;
    }
    return KEY_NONE;
}

static int parse_ss3(void)
{
    int c = next_byte(ESCDELAY);

    switch (c) {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    case 'H': return KEY_HOME;
    case 'F': return KEY_END;
    case 'P': return KEY_F(1);
    case 'Q': return KEY_F(2);
    case 'R': return KEY_F(3);
    case 'S': return KEY_F(4);
    case 'M': return KEY_ENTER;
    case 'j': return '*';
    case 'k': return '+';
    case 'l': return ',';
    case 'm': return '-';
    case 'n': return '.';
    case 'o': return '/';
    case 'p': return '0';
    case 'q': return '1';
    case 'r': return '2';
    case 's': return '3';
    case 't': return '4';
    case 'u': return '5';
    case 'v': return '6';
    case 'w': return '7';
    case 'x': return '8';
    case 'y': return '9';
    case 'X': return '=';
    default: break;
    }
    return KEY_NONE;
}

static int parse_escape(void)
{
    int c = next_byte(ESCDELAY);

    if (c < 0) return 27;
    if (c == '[') return parse_csi();
    if (c == 'O') return parse_ss3();
    unget_byte(c);
    return 27;
}

static int parse_key(void)
{
    int c = next_byte(0);

    if (c < 0) return KEY_NONE;
    if (c == 0x1b) return parse_escape();
    if (c == 0x7f || c == 0x08) return KEY_BACKSPACE;
    if (c == '\r' || c == '\n') return '\n';
    return c;
}

static int lp_getkey(int timeout_ms)
{
    long long deadline = timeout_ms < 0 ? -1 : lp_now_ms() + timeout_ms;

    for (;;) {
        int tmo;
        int r;
        int key;

        if (_nkeypush > 0) return _keypush[--_nkeypush];
        if (lp_detect_resize()) return KEY_RESIZE;

        if (deadline < 0) {
            tmo = 200;
        } else {
            long long rem = deadline - lp_now_ms();
            if (rem <= 0) tmo = 0;
            else tmo = rem > 200 ? 200 : (int)rem;
        }

        r = fill_input(tmo);
        if (r == 0) {
            if (deadline >= 0 && lp_now_ms() >= deadline) return ERR;
            continue;
        }
        if (r < 0) return ERR;

        key = parse_key();
        if (key != KEY_NONE) return key;
    }
}

int wgetch(WINDOW *win)
{
    int key;

    if (win == NULL || stdscr == NULL) return ERR;
    if (lp_ensure_active() != OK) return ERR;

    if (_resize_event) {
        _resize_event = 0;
        return KEY_RESIZE;
    }

    key = lp_getkey(WIN(win)->delay);
    if (key == ERR || key == KEY_RESIZE) return key;

    if (_echo_mode && key >= 32 && key < 127) {
        (void)waddch(win, (chtype)key);
        (void)wrefresh(win);
    }
    return key;
}

int getch(void)
{
    return wgetch(stdscr);
}

int ungetch(int ch)
{
    if (_nkeypush >= (int)sizeof(_keypush)) return ERR;
    _keypush[_nkeypush++] = ch;
    return OK;
}

int flushinp(void)
{
    _nkeypush = 0;
    _nbpush = 0;
    _inpos = _inlen = 0;

    for (;;) {
        struct pollfd pfd;
        char tmp[64];
        ssize_t n;

        pfd.fd = 0;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (poll(&pfd, 1, 0) <= 0) break;
        n = read(0, tmp, sizeof(tmp));
        if (n <= 0) break;
    }
    return OK;
}

int beep(void)
{
    lp_out_ch('\a');
    lp_out_flush();
    return OK;
}

int flash(void)
{
    lp_out_str("\x1b[?5h");
    lp_out_flush();
    (void)usleep(50000);
    lp_out_str("\x1b[?5l");
    lp_out_flush();
    return OK;
}
