# PROJECT KNOWLEDGE BASE — lpcurses

`lpcurses` is a freestanding, ncurses-source-compatible curses subset for
LeopardOS, distributed under the MIT License (`LICENSE`).  Programs include `curses.h` (or `ncurses.h`), link
`liblpcurses.a`, and run on a `leopard-conhost` PTY slave.  The backend is
direct ANSI/xterm-256color on fd 0 (input) and fd 1 (output); there is no
terminfo, termcap, ncurses, or curses dependency.

## Build and package

```bash
python3 gen_ninja.py
ninja build
PATH=../leopard-xbps/out:$PATH ninja pack
```

- Compiler: `clang`, freestanding flags from `gen_ninja.py`.
- CRT: `../XJ380-SDK/out/crt/lib/x86_64/{crt0.o,start.o,libxact.a,liballoc.a}`.
- `gen_ninja.py` discovers all `src/*.c` files and archives them into
  `out/liblpcurses.a`; keep one translation unit per functional area.
- `ninja pack` stages `system/include/curses.h`, `system/include/ncurses.h`,
  `system/lib/liblpcurses.a`, and `apps/system/lpcurses-demo.elf`; the
  generated command uses the local `xbps-create` when it is not in PATH.

## Module map

| File | Owns |
|------|------|
| `src/internal.h` | `struct lpc_cell`, `struct lpc_win_internal`, shared globals, internal prototypes |
| `src/global.c` | `stdscr`, `curscr`, `LINES`, `COLS`, `COLORS`, `COLOR_PAIRS`, pending/refresh state, saved termios |
| `src/screen.c` | `initscr`, `endwin`, `isendwin`, `curs_set`, termios mode application, alternate screen, size query/resize detection |
| `src/window.c` | window allocation/free, `newwin`/`subwin`/`derwin`, cursor/geometry getters, erase/clear/clearok/touchwin, screen-buffer resize |
| `src/output.c` | `addch`/`addstr`/`printw`, insert/delete, clear-to-EOL/EOF, scroll, attributes |
| `src/refresh.c` | output buffer, ANSI cursor/SGR emission, `wnoutrefresh`, `doupdate`, `wrefresh`, `refresh` |
| `src/box.c` | box/border/hline/vline |
| `src/input.c` | termios modes, keypad, timeout/nodelay/halfdelay, escape/key decoding, `getch`, `flushinp`, beep/flash |
| `src/color.c` | color-pair table and color API |
| `src/string.c` | `getnstr`/`wgetnstr` and the basic `scanw` implementation |
| `src/util.c` | UTF-8 decode/encode, display width, cell store/blank helpers, monotonic clock |

## Boundaries and invariants

- **Public header is the contract.** `include/curses.h` keeps the ncurses
  names, macros, key codes, and classic 32-bit attribute layout.  The public
  `WINDOW` fields (`_cury`, `_curx`, `_maxy`, `_maxx`, `_begy`, `_begx`,
  `_attrs`, `_bkgd`) are intentionally visible because ncurses `getyx`-style
  macros touch them.  All other per-window state is `_internal`.
- **Do not expose private state in the public header.** Add shared internals
  to `src/internal.h` and definitions to the owning module; avoid duplicate
  static copies across translation units.
- **fd 0/1 only.** Input is polled on fd 0; output is buffered and written to
  fd 1.  Do not open `/dev/tty` or assume a particular PTY device name.
- **Terminal ownership.** `initscr` saves termios and enters the alternate
  screen (`\x1b[?1049h`); `endwin` restores termios, shows the cursor, resets
  keypad mode, and leaves the alternate screen.  `refresh`/`getch` re-enter
  program mode after `endwin`.
- **No tty echo.** The driver's `ECHO` is always clear; `echo()`/`noecho()`
  control software echo inside `getch`.
- **Cell model.** A cell stores a Unicode scalar plus attributes.  Width-2
  characters occupy a head cell and a following `ch == 0` tail cell.  `addch`
  uses the low `A_CHARTEXT` byte for ncurses compatibility; UTF-8 is decoded
  by string functions.
- **Refresh.** `wnoutrefresh` composes into the internal `_newscr`;
  `doupdate` compares against `curscr` and emits only changed/forced cells,
  with ANSI cursor addressing and SGR.  `touchwin`/`redrawwin` set a force
  bit; `clearok`/`wclear` request a full clear.
- **Resize.** There is no SIGWINCH handler.  `getch` and `doupdate` poll
  `TIOCGWINSZ`; `lp_resize_screen` reallocates the screen buffers and re-bases
  windows that share `stdscr`'s buffer.  `getch` returns `KEY_RESIZE` once.
- **Colors.** Pair numbers live in the classic 8-bit `A_COLOR` field.  Pair 0
  and uninitialized pairs use terminal defaults (`39`/`49`); 0-15 use ANSI
  SGR, 16-255 use `38;5`/`48;5`.
- **Boxes.** Default `box`/`border` are ASCII `+ - |`.  `A_ALTCHARSET` switches
  to DEC special graphics (`\x1b(0` / `\x1b(B`) in the renderer.

## Testing

- `ninja build` builds the library and `tests/demo.c`.
- The demo is interactive: arrows/`j`/`k`, F1, `q`.
- Run the demo on LeopardOS as `/apps/system/lpcurses-demo.elf`.
- For host-side smoke tests, compile `tests/demo.c src/*.c` with the host
  compiler (without the freestanding flags) and run it under a PTY; this
  exercises the same ANSI/termios code paths.
