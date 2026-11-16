/*
 * lpcurses demo: a small interactive ncurses-style program.
 *
 * Build:  ninja build
 * Run:    /apps/system/lpcurses-demo.elf
 */
#include <curses.h>

#include <string.h>

static const char *const menu_items[] = {
    "Newwin / box / border",
    "Colored attributes",
    "Keyboard input",
    "Window scrolling",
    "Refresh and cursor",
    "Leave with q",
};
#define MENU_COUNT ((int)(sizeof(menu_items) / sizeof(menu_items[0])))

static WINDOW *make_window(void)
{
    int h;
    int w;
    int y;
    int x;

    if (LINES < 8 || COLS < 24)
        return newwin(LINES, COLS, 0, 0);

    h = LINES - 4;
    if (h > 16) h = 16;
    if (h < 6) h = LINES;

    w = COLS - 8;
    if (w > 62) w = 62;
    if (w < 20) w = COLS;

    y = (LINES - h) / 2;
    x = (COLS - w) / 2;
    return newwin(h, w, y, x);
}

static void draw_screen(WINDOW *win, int selected)
{
    int h;
    int w;
    int i;
    int title_x;

    getmaxyx(win, h, w);

    werase(win);
    box(win, 0, 0);

    title_x = (w - 16) / 2;
    if (title_x < 1) title_x = 1;
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, 0, title_x, " lpcurses demo ");
    wattroff(win, A_BOLD | COLOR_PAIR(1));

    if (h > 4) {
        mvwprintw(win, 2, 2, "Use Up/Down or j/k, q quits.");
        mvwprintw(win, 3, 2, "A simple direct-ANSI curses layer.");
    }

    for (i = 0; i < MENU_COUNT; i++) {
        int row = 5 + i;
        if (row >= h - 1) break;
        if (i == selected) {
            wattron(win, A_REVERSE | A_BOLD | COLOR_PAIR(2));
            mvwprintw(win, row, 2, " > %-36s", menu_items[i]);
            wattroff(win, A_REVERSE | A_BOLD | COLOR_PAIR(2));
        } else {
            wattron(win, COLOR_PAIR(3));
            mvwprintw(win, row, 2, "   %-36s", menu_items[i]);
            wattroff(win, COLOR_PAIR(3));
        }
    }

    if (h >= 3) {
        wattron(win, A_DIM);
        mvwprintw(win, h - 2, 2, "stdscr %dx%d  window %dx%d",
                  LINES, COLS, h, w);
        wattroff(win, A_DIM);
    }

    wrefresh(win);
}

static void init_demo_colors(void)
{
    if (!has_colors()) return;
    if (start_color() != OK) return;
    (void)use_default_colors();
    (void)init_pair(1, COLOR_CYAN, -1);
    (void)init_pair(2, COLOR_BLACK, COLOR_CYAN);
    (void)init_pair(3, COLOR_WHITE, -1);
}

int main(void)
{
    WINDOW *win;
    int selected = 0;
    int ch;

    if (initscr() == NULL) return 1;

    (void)cbreak();
    (void)noecho();
    (void)keypad(stdscr, TRUE);
    init_demo_colors();
    (void)curs_set(1);
    (void)clearok(stdscr, TRUE);

    win = make_window();
    if (win == NULL) {
        endwin();
        return 1;
    }
    draw_screen(win, selected);

    for (;;) {
        ch = wgetch(win);

        if (ch == 'q' || ch == 'Q') break;

        switch (ch) {
        case KEY_UP:
        case 'k':
            selected--;
            if (selected < 0) selected = MENU_COUNT - 1;
            break;
        case KEY_DOWN:
        case 'j':
            selected++;
            if (selected >= MENU_COUNT) selected = 0;
            break;
        case KEY_HOME:
            selected = 0;
            break;
        case KEY_END:
            selected = MENU_COUNT - 1;
            break;
        case KEY_RESIZE:
            (void)delwin(win);
            (void)clearok(stdscr, TRUE);
            win = make_window();
            if (win == NULL) {
                endwin();
                return 1;
            }
            selected = 0;
            break;
        case KEY_F(1):
            (void)flash();
            break;
        default:
            break;
        }

        draw_screen(win, selected);
    }

    (void)delwin(win);
    (void)endwin();
    return 0;
}
