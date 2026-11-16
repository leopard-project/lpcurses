# lpcurses

`lpcurses` is distributed under the **MIT License**.  See `LICENSE`.

`lpcurses` is a small **ncurses-source-compatible** curses subset for
LeopardOS.  It is not an ncurses ABI or terminfo replacement: programs compile
against the provided `curses.h`/`ncurses.h`, link `liblpcurses.a`, and talk
directly to an ANSI/xterm-256color terminal on file descriptors 0 and 1.

The normal execution environment is a `leopard-conhost` PTY slave with
`fd 0/1/2` and an initially canonical termios.  `lpcurses` owns the alternate
screen, cursor visibility, termios mode switching, and ANSI repainting.

## Build

```bash
cd lpcurses
python3 gen_ninja.py
ninja build
ninja pack            # xbps-create; may need ../leopard-xbps/out in PATH
```

Outputs:

- `out/liblpcurses.a`
- `out/lpcurses-demo.elf`
- `out/lpcurses-1.0_2.x86_64.xbps`

The library is freestanding and links against the XJ380 SDK CRT:

```text
../XJ380-SDK/out/crt/lib/x86_64/
    crt0.o start.o libxact.a liballoc.a
```

The demo link line is:

```text
crt0.o start.o demo.o liblpcurses.a libxact.a liballoc.a
```

## Package layout

```text
system/include/curses.h
system/include/ncurses.h
system/lib/liblpcurses.a
apps/system/lpcurses-demo.elf
system/licenses/lpcurses/LICENSE
```

## Demo

On a LeopardOS system after installing the package:

```text
/apps/system/lpcurses-demo.elf
```

Use Up/Down or `j`/`k` to move the selection, F1 to flash the screen, and
`q` to leave.  The demo calls `initscr`, `cbreak`, `noecho`, `keypad`, draws a
boxed window with colors, reads keys with `wgetch`, handles `KEY_RESIZE`, and
restores the terminal with `endwin`.

## Source layout

| Module | Responsibility |
|--------|----------------|
| `src/internal.h` | Private cell/window state, shared globals, helper declarations |
| `src/global.c` | Public globals (`stdscr`, `curscr`, `LINES`, `COLS`, colors), saved terminal state |
| `src/screen.c` | `initscr`/`endwin`/`isendwin`, termios save/restore, alternate screen, cursor visibility, resize detection |
| `src/window.c` | Window allocation (`newwin`/`subwin`/`derwin`/`delwin`), geometry, erase/clear, screen buffer resize |
| `src/output.c` | `addch`/`addstr`/`printw`, editing, scrolling, insert/delete, attributes |
| `src/refresh.c` | Output buffering, ANSI diff emission, `wnoutrefresh`/`doupdate`/`wrefresh` |
| `src/box.c` | `box`/`border`/`hline`/`vline` |
| `src/input.c` | `getch`/`wgetch`, keypad/escape decoding, timeouts, cbreak/raw/echo, `flushinp`, beep/flash |
| `src/color.c` | Color pairs, `start_color`, `init_pair`, `pair_content`, `use_default_colors` |
| `src/string.c` | `getnstr`/`wgetnstr`, `scanw`/`wscanw`/`mvscanw` and the small scanf |
| `src/util.c` | UTF-8, character width, cell helpers, monotonic time |

`include/curses.h` keeps the ncurses-style public `WINDOW` geometry/attribute
fields so `getyx`, `getbegyx`, and `getmaxyx` work unchanged.  The cell buffer
and dirty flags are private behind `_internal`.

## Covered API

The implementation covers the lifecycle, cursor/window geometry, window
creation, output, editing, attributes, colors, input modes, key decoding,
`getnstr`, and basic `scanw`/`printw` formatting needed by ordinary narrow
ncurses programs.  See `include/curses.h` for the exact prototypes, macros,
`A_*` attributes, `COLOR_PAIR`, `ACS_*` characters, and `KEY_*` values.

## Non-goals and limitations

- No terminfo database, no `TERM` capability lookup, no ncurses ABI.
- No `cchar_t`/wide-character API (`addwstr`, `get_wch`, `wattr_set`, ...).
- No panels, menus, forms, mouse, soft labels, or termcap/terminfo files.
- `chtype` uses the classic 32-bit ncurses layout: `addch()` is byte-oriented.
  UTF-8 is decoded by `addstr`/`addnstr`, and each cell stores a Unicode
  scalar.  Code points above `U+00FF` must go through strings.
- Colors are ANSI pairs (`COLOR_PAIR(n)`, up to 255); `COLORS`/`COLOR_PAIRS`
  report 256 and the backend emits xterm 256-color SGR for indices 16-255.
- `A_ALTCHARSET` emits the standard DEC special-graphics charset; default
  boxes use plain ASCII `+ - |`.
- Resize handling re-bases subwindows onto the new `stdscr` buffer.  Windows
  created independently with `newwin` keep their original size.
- There is no SIGWINCH handler; size changes are detected by polling
  `TIOCGWINSZ` in `getch`/`doupdate`.
