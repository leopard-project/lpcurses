/*
 * lpcurses - a small ncurses-source-compatible curses subset
 * for LeopardOS / XJ380 terminals.
 *
 * This is a source-level compatibility layer, not an ncurses ABI or
 * terminfo replacement.  The backend is direct ANSI/xterm output on
 * file descriptors 0 and 1.
 */
#ifndef LPCURSES_CURSES_H
#define LPCURSES_CURSES_H

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LPCURSES_VERSION "1.0"

typedef unsigned int chtype;
typedef chtype         attr_t;
typedef unsigned int   mmask_t;

struct lpc_win_internal;

/*
 * The public part of WINDOW intentionally mirrors the field names used by
 * ncurses (getyx()/getmaxyx() are normally macros that touch them).  Programs
 * that only use the documented accessors do not need to know the layout.
 * The private cell buffer and flags live behind _internal in src/internal.h.
 */
typedef struct _win {
    int            _cury;          /* current cursor row */
    int            _curx;          /* current cursor column */
    int            _maxy;          /* number of rows */
    int            _maxx;          /* number of columns */
    int            _begy;          /* screen row of upper-left corner */
    int            _begx;          /* screen column of upper-left corner */
    int            _flags;
    attr_t         _attrs;
    chtype         _bkgd;
    struct lpc_win_internal *_internal;
} WINDOW;

typedef struct _screen SCREEN;

extern WINDOW *stdscr;
extern WINDOW *curscr;

extern int LINES;
extern int COLS;
extern int COLORS;
extern int COLOR_PAIRS;

/* Milliseconds to wait for a continuation after ESC. */
extern int ESCDELAY;

#define OK   0
#define ERR  (-1)

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* ------------------------------------------------------------------ */
/* Attributes                                                          */
/* ------------------------------------------------------------------ */

/*
 * These are the classic narrow ncurses bit assignments: the low byte is
 * the character, the next byte is the color pair, and the remaining bits
 * are attributes.  addch() is therefore byte-oriented, as in ncurses;
 * UTF-8 strings passed to addstr()/addnstr() are decoded into cells.
 */
#define A_NORMAL     0x00000000U
#define A_ATTRIBUTES 0xffffff00U
#define A_CHARTEXT   0x000000ffU
#define A_COLOR      0x0000ff00U
#define A_STANDOUT   0x00010000U
#define A_UNDERLINE  0x00020000U
#define A_REVERSE    0x00040000U
#define A_BLINK      0x00080000U
#define A_DIM        0x00100000U
#define A_BOLD       0x00200000U
#define A_ALTCHARSET 0x00400000U
#define A_INVIS      0x00800000U
#define A_PROTECT    0x01000000U

#define COLOR_PAIR(n)  ((chtype)(((unsigned int)(n) & 0xffU) << 8))
#define PAIR_NUMBER(a) ((int)(((unsigned int)(a) & A_COLOR) >> 8))

/* ------------------------------------------------------------------ */
/* Colors                                                              */
/* ------------------------------------------------------------------ */

#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_YELLOW  3
#define COLOR_BLUE    4
#define COLOR_MAGENTA 5
#define COLOR_CYAN    6
#define COLOR_WHITE   7

/* ------------------------------------------------------------------ */
/* Keys                                                               */
/* ------------------------------------------------------------------ */

#define KEY_CODE_YES 0400
#define KEY_MIN      0401
#define KEY_BREAK    0401
#define KEY_DOWN     0402
#define KEY_UP       0403
#define KEY_LEFT     0404
#define KEY_RIGHT    0405
#define KEY_HOME     0406
#define KEY_BACKSPACE 0407
#define KEY_F0       0410
#define KEY_F(n)     (KEY_F0 + (n))
#define KEY_DL       0510
#define KEY_IL       0511
#define KEY_DC       0512
#define KEY_IC       0513
#define KEY_EIC      0514
#define KEY_CLEAR    0515
#define KEY_EOS      0516
#define KEY_EOL      0517
#define KEY_SF       0520
#define KEY_SR       0521
#define KEY_NPAGE    0522
#define KEY_PPAGE    0523
#define KEY_STAB     0524
#define KEY_CTAB     0525
#define KEY_CATAB    0526
#define KEY_ENTER    0527
#define KEY_SRESET   0530
#define KEY_RESET    0531
#define KEY_PRINT    0532
#define KEY_LL       0533
#define KEY_A1       0534
#define KEY_A3       0535
#define KEY_B2       0536
#define KEY_C1       0537
#define KEY_C3       0540
#define KEY_BTAB     0541
#define KEY_BEG      0542
#define KEY_CANCEL   0543
#define KEY_CLOSE    0544
#define KEY_COMMAND  0545
#define KEY_COPY     0546
#define KEY_CREATE   0547
#define KEY_END      0550
#define KEY_EXIT     0551
#define KEY_FIND     0552
#define KEY_HELP     0553
#define KEY_MARK     0554
#define KEY_MESSAGE  0555
#define KEY_MOVE     0556
#define KEY_NEXT     0557
#define KEY_OPEN     0560
#define KEY_OPTIONS  0561
#define KEY_PREVIOUS 0562
#define KEY_REDO     0563
#define KEY_REFERENCE 0564
#define KEY_REFRESH  0565
#define KEY_REPLACE  0566
#define KEY_RESTART  0567
#define KEY_RESUME   0570
#define KEY_SAVE     0571
#define KEY_SBEG     0572
#define KEY_SCANCEL  0573
#define KEY_SCOMMAND 0574
#define KEY_SCOPY    0575
#define KEY_SCREATE  0576
#define KEY_SDC      0577
#define KEY_SDL      0600
#define KEY_SELECT   0601
#define KEY_SEND     0602
#define KEY_SEOL     0603
#define KEY_SEXIT    0604
#define KEY_SFIND    0605
#define KEY_SHELP    0606
#define KEY_SHOME    0607
#define KEY_SIC      0610
#define KEY_SLEFT    0611
#define KEY_SMESSAGE 0612
#define KEY_SMOVE    0613
#define KEY_SNEXT    0614
#define KEY_SOPTIONS 0615
#define KEY_SPREVIOUS 0616
#define KEY_SPRINT   0617
#define KEY_SREDO    0620
#define KEY_SREPLACE 0621
#define KEY_SRIGHT   0622
#define KEY_SRSUME   0623
#define KEY_SSAVE    0624
#define KEY_SSUSPEND 0625
#define KEY_SUNDO    0626
#define KEY_SUSPEND  0627
#define KEY_UNDO     0630
#define KEY_MOUSE    0631
#define KEY_RESIZE   0632
#define KEY_MAX      0777

/* ------------------------------------------------------------------ */
/* Alternate character set (DEC special graphics)                      */
/* ------------------------------------------------------------------ */

#define ACS_ULCORNER (A_ALTCHARSET | 'l')
#define ACS_LLCORNER (A_ALTCHARSET | 'm')
#define ACS_URCORNER (A_ALTCHARSET | 'k')
#define ACS_LRCORNER (A_ALTCHARSET | 'j')
#define ACS_LTEE     (A_ALTCHARSET | 't')
#define ACS_RTEE     (A_ALTCHARSET | 'u')
#define ACS_BTEE     (A_ALTCHARSET | 'v')
#define ACS_TTEE     (A_ALTCHARSET | 'w')
#define ACS_HLINE    (A_ALTCHARSET | 'q')
#define ACS_VLINE    (A_ALTCHARSET | 'x')
#define ACS_PLUS     (A_ALTCHARSET | 'n')
#define ACS_S1       (A_ALTCHARSET | 'o')
#define ACS_S9       (A_ALTCHARSET | 's')
#define ACS_DIAMOND  (A_ALTCHARSET | '`')
#define ACS_CKBOARD  (A_ALTCHARSET | 'a')
#define ACS_DEGREE   (A_ALTCHARSET | 'f')
#define ACS_PLMINUS  (A_ALTCHARSET | 'g')
#define ACS_BULLET   (A_ALTCHARSET | '~')
#define ACS_LARROW   (A_ALTCHARSET | ',')
#define ACS_RARROW   (A_ALTCHARSET | '+')
#define ACS_DARROW   (A_ALTCHARSET | '.')
#define ACS_UARROW   (A_ALTCHARSET | '-')
#define ACS_BOARD    (A_ALTCHARSET | 'h')
#define ACS_LANTERN  (A_ALTCHARSET | 'i')
#define ACS_BLOCK    (A_ALTCHARSET | '0')

/* ------------------------------------------------------------------ */
/* Lifecycle / screen                                                  */
/* ------------------------------------------------------------------ */

WINDOW *initscr(void);
int     endwin(void);
bool    isendwin(void);

int refresh(void);
int wrefresh(WINDOW *win);
int wnoutrefresh(WINDOW *win);
int doupdate(void);

int clear(void);
int erase(void);
int werase(WINDOW *win);
int wclear(WINDOW *win);
int clearok(WINDOW *win, bool bf);
int touchwin(WINDOW *win);
int redrawwin(WINDOW *win);

/* ------------------------------------------------------------------ */
/* Cursor / window geometry                                            */
/* ------------------------------------------------------------------ */

int move(int y, int x);
int wmove(WINDOW *win, int y, int x);
int getcury(const WINDOW *win);
int getcurx(const WINDOW *win);
int getbegy(const WINDOW *win);
int getbegx(const WINDOW *win);
int getmaxy(const WINDOW *win);
int getmaxx(const WINDOW *win);

#define getyx(win, y, x)    ((y) = (win)->_cury, (x) = (win)->_curx)
#define getbegyx(win, y, x) ((y) = (win)->_begy, (x) = (win)->_begx)
#define getmaxyx(win, y, x) ((y) = (win)->_maxy, (x) = (win)->_maxx)

/* ------------------------------------------------------------------ */
/* Windows                                                             */
/* ------------------------------------------------------------------ */

WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x);
WINDOW *subwin(WINDOW *orig, int nlines, int ncols, int begin_y, int begin_x);
WINDOW *derwin(WINDOW *orig, int nlines, int ncols, int begin_y, int begin_x);
int     delwin(WINDOW *win);

int box(WINDOW *win, chtype verch, chtype horch);
int border(WINDOW *win, chtype ls, chtype rs, chtype ts, chtype bs,
           chtype tl, chtype tr, chtype bl, chtype br);
int wborder(WINDOW *win, chtype ls, chtype rs, chtype ts, chtype bs,
            chtype tl, chtype tr, chtype bl, chtype br);
int hline(chtype ch, int n);
int whline(WINDOW *win, chtype ch, int n);
int vline(chtype ch, int n);
int wvline(WINDOW *win, chtype ch, int n);

/* ------------------------------------------------------------------ */
/* Output                                                              */
/* ------------------------------------------------------------------ */

int addch(chtype ch);
int waddch(WINDOW *win, chtype ch);
int addstr(const char *str);
int waddstr(WINDOW *win, const char *str);
int addnstr(const char *str, int n);
int waddnstr(WINDOW *win, const char *str, int n);

int mvaddch(int y, int x, chtype ch);
int mvwaddch(WINDOW *win, int y, int x, chtype ch);
int mvaddstr(int y, int x, const char *str);
int mvwaddstr(WINDOW *win, int y, int x, const char *str);
int mvaddnstr(int y, int x, const char *str, int n);
int mvwaddnstr(WINDOW *win, int y, int x, const char *str, int n);

int printw(const char *fmt, ...);
int wprintw(WINDOW *win, const char *fmt, ...);
int mvprintw(int y, int x, const char *fmt, ...);
int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...);

int echochar(chtype ch);
int wechochar(WINDOW *win, chtype ch);

/* ------------------------------------------------------------------ */
/* Editing                                                             */
/* ------------------------------------------------------------------ */

int insch(chtype ch);
int winsch(WINDOW *win, chtype ch);
int delch(void);
int wdelch(WINDOW *win);
int clrtoeol(void);
int wclrtoeol(WINDOW *win);
int clrtobot(void);
int wclrtobot(WINDOW *win);
int insertln(void);
int winsertln(WINDOW *win);
int deleteln(void);
int wdeleteln(WINDOW *win);
int scroll(WINDOW *win);
int wscrl(WINDOW *win, int n);
int scrollok(WINDOW *win, bool bf);
int idlok(WINDOW *win, bool bf);

/* ------------------------------------------------------------------ */
/* Attributes                                                          */
/* ------------------------------------------------------------------ */

int attron(chtype attrs);
int attroff(chtype attrs);
int attrset(chtype attrs);
int wattron(WINDOW *win, chtype attrs);
int wattroff(WINDOW *win, chtype attrs);
int wattrset(WINDOW *win, chtype attrs);
int standout(void);
int standend(void);

/* ------------------------------------------------------------------ */
/* Colors                                                              */
/* ------------------------------------------------------------------ */

int  start_color(void);
bool has_colors(void);
bool can_change_color(void);
int  init_pair(short pair, short f, short b);
int  pair_content(short pair, short *f, short *b);
int  use_default_colors(void);

/* ------------------------------------------------------------------ */
/* Input / modes                                                       */
/* ------------------------------------------------------------------ */

int  cbreak(void);
int  nocbreak(void);
int  raw(void);
int  noraw(void);
int  echo(void);
int  noecho(void);
int  keypad(WINDOW *win, bool bf);
int  curs_set(int visibility);
int  getch(void);
int  wgetch(WINDOW *win);
int  ungetch(int ch);
int  timeout(int delay);
int  wtimeout(WINDOW *win, int delay);
int  nodelay(WINDOW *win, bool bf);
int  halfdelay(int tenths);
int  flushinp(void);
int  beep(void);
int  flash(void);

/* ------------------------------------------------------------------ */
/* String input / scanning                                             */
/* ------------------------------------------------------------------ */

int getnstr(char *str, int n);
int wgetnstr(WINDOW *win, char *str, int n);
int scanw(const char *fmt, ...);
int wscanw(WINDOW *win, const char *fmt, ...);
int mvscanw(int y, int x, const char *fmt, ...);
int mvwscanw(WINDOW *win, int y, int x, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* LPCURSES_CURSES_H */
