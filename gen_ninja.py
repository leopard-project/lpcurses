#!/usr/bin/env python3
"""lpcurses: freestanding curses subset library + demo + xbps packaging.

Usage:
    python3 gen_ninja.py
    ninja build
    ninja pack
"""
import os
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "out"

LIB_SRCS = sorted((ROOT / "src").glob("*.c"))
DEMO_SRC = ROOT / "tests" / "demo.c"

LIB = OUT / "liblpcurses.a"
DEMO_OBJ = OUT / "demo.c.o"
DEMO = OUT / "lpcurses-demo.elf"

# XACT CRT (../leopard-sdk/out/crt): startup objects + static runtime + allocator.
CRT_ROOT = ROOT / ".." / "leopard-sdk"
INC = CRT_ROOT / "out" / "crt" / "include"
LIBDIR = CRT_ROOT / "out" / "crt" / "lib" / "x86_64"
CRT_OBJS = [LIBDIR / "crt0.o", LIBDIR / "start.o"]
CRT_LIBS = [LIBDIR / "libxact.a", LIBDIR / "liballoc.a"]

PKGNAME = "lpcurses"
PKGVER = "1.0_2"
ARCH = "x86_64"
DESC = "Small ncurses-source-compatible curses subset for LeopardOS"
PKG_OUT = OUT / f"{PKGNAME}-{PKGVER}.{ARCH}.xbps"


def rel(p):
    p = Path(p)
    if p.is_absolute():
        try:
            return str(p.relative_to(ROOT))
        except ValueError:
            return str(p)
    return str(p)


def esc(p):
    return rel(p).replace("$", "$$").replace(":", "$:").replace(" ", "$ ")


def find_xbps_create():
    found = shutil.which("xbps-create")
    if found:
        return found
    candidate = ROOT.parent / "leopard-xbps" / "out" / "xbps-create"
    if candidate.exists():
        return str(candidate)
    return "xbps-create"


def emit_package(L, xbps_create):
    # Install tree (relative to /).
    files = [
        (LIB, "system/lib/liblpcurses.a"),
        (DEMO, "apps/system/lpcurses-demo.elf"),
        (ROOT / "include" / "curses.h", "system/include/curses.h"),
        (ROOT / "include" / "ncurses.h", "system/include/ncurses.h"),
        (ROOT / "LICENSE", "system/licenses/lpcurses/LICENSE"),
    ]

    cmd = "rm -rf .pkg"
    for src, dst in files:
        cmd += (f" && mkdir -p .pkg/$$(dirname {dst})"
                f" && cp {esc(src)} .pkg/{dst}")
    cmd += (f" && cd out && {esc(xbps_create)} --compression gzip"
            f" -l MIT -A {ARCH} -n {PKGNAME}-{PKGVER} -s \"{DESC}\" ../.pkg")

    L += ["rule package", f"  command = {cmd}", ""]
    L += [f"build {esc(PKG_OUT)}: package "
          + " ".join(esc(src) for src, _ in files)]
    L += [f"build pack: phony {esc(PKG_OUT)}", ""]


def main():
    cc = os.environ.get("CC", "clang")
    ld = os.environ.get("LD", "ld")
    ar = os.environ.get("AR", "ar")
    opt = "-O" + os.environ.get("OPT_LEVEL", "2")
    col = os.environ.get("DIAGNOSTICS_COLOR", "-fdiagnostics-color=always")

    common = (f"{col} {opt} -Wall -g -ffreestanding -fno-builtin -m64 "
              "-mstackrealign -fno-stack-protector -fno-exceptions "
              "-fno-strict-aliasing -fshort-wchar -nostdinc")
    c_flags = f"{common} -std=c11 -I {rel(INC)} -I {rel(ROOT / 'include')} -I . -MMD -MP"

    L = ["# lpcurses build + xbps packaging",
         "ninja_required_version = 1.10",
         f"cc = {cc}", f"ld = {ld}", f"ar = {ar}",
         f"c_flags = {c_flags}", ""]
    L += [
        "rule cc",
        "  command = mkdir -p $$(dirname $out) && $cc $c_flags -MF $out.d -c $in -o $out",
        "  depfile = $out.d",
        "  deps = gcc",
        "",
        "rule ar",
        "  command = rm -f $out && $ar rcs $out $in",
        "",
        "rule link",
        "  command = $ld -Ttext=0x200000 $in -o $out",
        "",
    ]

    lib_objs = []
    for src in LIB_SRCS:
        obj = OUT / (src.name + ".o")
        L += [f"build {esc(obj)}: cc {esc(src)}", ""]
        lib_objs.append(obj)
    L += [f"build {esc(LIB)}: ar " + " ".join(esc(o) for o in lib_objs), ""]
    L += [f"build {esc(DEMO_OBJ)}: cc {esc(DEMO_SRC)}", ""]

    link_inputs = CRT_OBJS + [DEMO_OBJ, LIB] + CRT_LIBS
    L += [f"build {esc(DEMO)}: link "
          + " ".join(esc(x) for x in link_inputs), ""]

    L += [f"build build: phony {esc(LIB)} {esc(DEMO)}", "default build", ""]
    emit_package(L, find_xbps_create())
    L += ["rule clean", "  command = rm -rf out .pkg",
          "build clean: clean"]

    (ROOT / "build.ninja").write_text("\n".join(L) + "\n", encoding="utf-8")
    print("已生成 build.ninja（lpcurses）。用法：ninja build / ninja pack")


if __name__ == "__main__":
    main()
