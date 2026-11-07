#!/usr/bin/env python3
"""Runs a GFA-BASIC program (.lst) in the original interpreter inside FS-UAE on macOS and
collects its result file. Standard library only.

    python3 tools/gfa/gfa_run.py tests/probes/probe1.lst

How it works (docs/gfa-original.md):
  1. The program is copied into the shared directory, which FS-UAE mounts as the Amiga hard
     drive DH0:. Its `datei$="..."` line is rewritten to `DH0:<name>.txt`.
  2. FS-UAE is started with the given configuration unless it is already running.
  3. Key strokes are sent to the FS-UAE window with osascript (System Events): delete the
     program in the editor, Merge (F2) the file, Run (Shift+F10). The sequence is in
     tools/gfa/keys.json and can be adjusted there.
  4. The script waits until the result file contains a line "ende" and prints it.
  5. On a timeout a screenshot is saved next to the result, so that one can see where it hangs.

Key strokes are sent as physical key codes. FS-UAE passes key *positions* to the Amiga, and the
Amiga keymap decides which character appears; --amiga-layout selects the table (de or us).

Needs: macOS, FS-UAE, and for the terminal (or Claude Code) the permission
"Bedienungshilfen / Accessibility" in the system settings, otherwise System Events refuses the
key strokes.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_SHARE = Path.home() / "Documents" / "FS-UAE" / "gfa-share"
DEFAULT_CONFIG = HERE / "gfa.fs-uae"
DEFAULT_KEYS = HERE / "keys.json"
# The GFA-BASIC 3.5 disk boots with the US keymap.
DEFAULT_LAYOUT = "us"

# ---- key codes ---------------------------------------------------------------------------------
# macOS virtual key codes are positions on an ANSI keyboard (the same position on an ISO German
# keyboard has the same code). FS-UAE forwards positions to the Amiga.
MAC_KEYCODES = {
    "a": 0, "s": 1, "d": 2, "f": 3, "h": 4, "g": 5, "z": 6, "x": 7, "c": 8, "v": 9, "b": 11,
    "q": 12, "w": 13, "e": 14, "r": 15, "y": 16, "t": 17, "1": 18, "2": 19, "3": 20, "4": 21,
    "6": 22, "5": 23, "=": 24, "9": 25, "7": 26, "-": 27, "8": 28, "0": 29, "o": 31, "u": 32,
    "i": 34, "p": 35, "l": 37, "j": 38, "k": 40, ";": 41, ",": 43, "/": 44, "n": 45, "m": 46,
    ".": 47,
}
NAMED_KEYS = {
    "return": 36, "tab": 48, "space": 49, "backspace": 51, "escape": 53, "delete": 117,
    "f1": 122, "f2": 120, "f3": 99, "f4": 118, "f5": 96, "f6": 97, "f7": 98, "f8": 100,
    "f9": 101, "f10": 109, "left": 123, "right": 124, "down": 125, "up": 126, "help": 114,
}

# Which physical key (named by its US position) and shift state produce a character in the
# Amiga keymap. Only the characters needed for file names and simple answers are listed.
def _layout_us() -> dict[str, tuple[str, bool]]:
    table = {c: (c, False) for c in "abcdefghijklmnopqrstuvwxyz0123456789.-/"}
    table.update({":": (";", True), "_": ("-", True)})
    return table


def _layout_de() -> dict[str, tuple[str, bool]]:
    table = {c: (c, False) for c in "abcdefghijklmnopqrstuvwxyz0123456789.-"}
    table["y"] = ("z", False)  # Y and Z are swapped on the German keymap
    table["z"] = ("y", False)
    table["-"] = ("/", False)  # '-' sits where the US '/' is
    table[":"] = (".", True)
    table["_"] = ("/", True)
    table["/"] = ("7", True)
    return table


LAYOUTS = {"us": _layout_us(), "de": _layout_de()}


def key_events(text: str, layout: str) -> list[tuple[int, bool]]:
    """Physical key code and shift state for every character of `text` (case-insensitive for
    letters; upper-case letters are typed with shift)."""
    table = LAYOUTS[layout]
    events = []
    for ch in text:
        low = ch.lower()
        if low not in table:
            raise ValueError(f"character {ch!r} cannot be typed with the {layout} table")
        key, shift = table[low]
        events.append((MAC_KEYCODES[key], shift or (ch.isalpha() and ch.isupper())))
    return events


def parse_step(step: str, layout: str) -> list[str]:
    """One entry of keys.json -> AppleScript lines.

    "key:f2", "key:shift+f10"   named key, optional modifiers (shift, ctrl, alt, cmd)
    "key:delete*30"             the same key pressed 30 times
    "type:dh0:probe1.lst"       characters through the layout table
    "wait:1.5"                  pause in seconds
    """
    kind, _, arg = step.partition(":")
    if kind == "wait":
        return [f"delay {float(arg)}"]
    if kind == "key":
        arg, _, count = arg.partition("*")
        *mods, name = arg.lower().split("+")
        if name not in NAMED_KEYS:
            raise ValueError(f"unknown key {name!r}")
        mod_map = {"shift": "shift down", "ctrl": "control down", "alt": "option down", "cmd": "command down"}
        using = ", ".join(mod_map[m] for m in mods)
        suffix = f" using {{{using}}}" if using else ""
        press = [f'tell application "System Events" to key code {NAMED_KEYS[name]}{suffix}', "delay 0.15"]
        return press * int(count or 1)
    if kind == "type":
        lines = []
        for code, shift in key_events(arg, layout):
            suffix = " using {shift down}" if shift else ""
            lines.append(f'tell application "System Events" to key code {code}{suffix}')
            lines.append("delay 0.08")
        return lines
    raise ValueError(f"unknown step {step!r}")


FOCUS_WAIT = """\
repeat 40 times
  tell application "{app}" to activate
  delay 0.25
  tell application "System Events" to set frontApp to name of first application process whose frontmost is true
  if frontApp contains "uae" or frontApp contains "UAE" then exit repeat
end repeat
if not (frontApp contains "uae" or frontApp contains "UAE") then error "FS-UAE does not get the focus"
delay 2"""


def build_script(steps: list[str], layout: str, app: str, variables: dict[str, str]) -> str:
    # Activate FS-UAE and wait until it really is frontmost; otherwise the keys go elsewhere.
    lines = [FOCUS_WAIT.replace("{app}", app)]
    for step in steps:
        for name, value in variables.items():
            step = step.replace("{" + name + "}", value)
        lines += parse_step(step, layout)
    return "\n".join(lines) + "\n"


# ---- the program -------------------------------------------------------------------------------
DATEI_RE = re.compile(r'^(\s*datei\$\s*=\s*)"[^"]*"', re.IGNORECASE | re.MULTILINE)


PRINT_RE = re.compile(r"^(\s*)PRINT\b\s*(.*)$", re.IGNORECASE)
END_RE = re.compile(r"^\s*END\s*(!.*)?$", re.IGNORECASE)
ROUTINE_RE = re.compile(r"^\s*(PROCEDURE|FUNCTION)\s+([A-Za-z0-9_.]+)", re.IGNORECASE)
ONERROR_RE = re.compile(r"^\s*ON\s*ERROR\s+GOSUB\s+([A-Za-z0-9_.]+)", re.IGNORECASE)


def redirect_screen_program(source: str, result_path: str) -> str:
    """Rewrites a program that prints to the screen (tests/programs) so that it prints into the
    result file: PRINT -> PRINT #1, END and the start of the procedures -> @zz.end (writes
    "ende", closes the file, ends). Error routines first reopen the file, because a trapped error
    makes the channel unusable on the original (docs/language.md)."""
    lines = source.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    handlers = {m.group(1).upper() for m in map(ONERROR_RE.match, lines) if m}
    out = [f'zz.out$="{result_path}"', 'OPEN "O",#1,zz.out$']
    in_main = True
    for line in lines:
        routine = ROUTINE_RE.match(line)
        if routine and in_main:
            out.append("@zz.end")
            in_main = False
        if END_RE.match(line):
            out.append("@zz.end")
            continue
        m = PRINT_RE.match(line)
        if m:
            rest = m.group(2)
            line = m.group(1) + ("PRINT #1," + rest if rest else "PRINT #1")
        out.append(line)
        if routine and routine.group(2).upper() in handlers:
            out += ["  zz.err%=ERR", "  CLOSE #1", '  OPEN "A",#1,zz.out$']
    while out and out[-1] == "":
        out.pop()
    if in_main:
        out.append("@zz.end")
    out += ["PROCEDURE zz.end", '  PRINT #1,"ende"', "  CLOSE #1", "  END", "RETURN"]
    return "\n".join(out) + "\n"


def strip_end_marker(text: str) -> str:
    """The result of a redirected program without its last line "ende"."""
    return re.sub(r"ende\n?$", "", text)


def prepare_program(source: str, result_path: str) -> str:
    """Points the program's `datei$="..."` line to the result file (probe programs) or redirects
    its screen output into the file (test programs), and normalises line ends."""
    if DATEI_RE.search(source):
        text = DATEI_RE.sub(lambda m: m.group(1) + '"' + result_path + '"', source, count=1)
    else:
        text = redirect_screen_program(source, result_path)
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    try:
        text.encode("ascii")
    except UnicodeEncodeError as e:
        raise ValueError("the program must be plain ASCII (no umlauts)") from e
    return text if text.endswith("\n") else text + "\n"


def amiga_name(stem: str) -> str:
    name = re.sub(r"[^a-z0-9]", "", stem.lower())
    if not name:
        raise ValueError("file name must contain letters or digits")
    return name[:24]


# ---- FS-UAE ------------------------------------------------------------------------------------
def fs_uae_running() -> bool:
    return subprocess.run(["pgrep", "-x", "fs-uae"], capture_output=True).returncode == 0


def start_fs_uae(config: Path, app: str) -> None:
    subprocess.run(["open", "-a", app, "--args", str(config)], check=True)


def wait_for_result(path: Path, timeout: float) -> str | None:
    end = time.time() + timeout
    while time.time() < end:
        if path.exists():
            text = path.read_text(errors="replace").replace("\r", "")
            if re.search(r"^ende\s*$", text, re.MULTILINE):
                return text
        time.sleep(0.5)
    return None


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("program", type=Path, help="GFA-BASIC program (.lst, ASCII)")
    p.add_argument("--share", type=Path, default=Path(os.environ.get("GFA_SHARE", DEFAULT_SHARE)),
                   help="Mac directory mounted as DH0: (default ~/Documents/FS-UAE/gfa-share)")
    p.add_argument("--config", type=Path, default=Path(os.environ.get("GFA_FSUAE_CONFIG", DEFAULT_CONFIG)),
                   help="FS-UAE configuration")
    p.add_argument("--keys", type=Path, default=DEFAULT_KEYS, help="key sequence (JSON)")
    p.add_argument("--amiga-layout", choices=sorted(LAYOUTS), default=os.environ.get("GFA_LAYOUT", DEFAULT_LAYOUT))
    p.add_argument("--app", default="FS-UAE", help="name of the macOS application")
    p.add_argument("--boot-wait", type=float, default=25.0,
                   help="seconds to wait after starting FS-UAE until the GFA editor is ready")
    p.add_argument("--timeout", type=float, default=120.0, help="seconds to wait for the result")
    p.add_argument("--out", type=Path, help="copy the result here as well")
    p.add_argument("--screenshot", action="store_true", help="always save a screenshot after the run")
    p.add_argument("--dry-run", action="store_true", help="only print the prepared program and the AppleScript")
    args = p.parse_args(argv)

    name = amiga_name(args.program.stem)
    lst_name, txt_name = f"{name}.lst", f"{name}.txt"
    program = prepare_program(args.program.read_text(), f"DH0:{txt_name}")
    keys = json.loads(args.keys.read_text())
    script = build_script(keys["run"], args.amiga_layout, args.app, {"file": f"dh0:{lst_name}"})

    if args.dry_run:
        print(program)
        print("---- AppleScript ----")
        print(script)
        return 0
    if sys.platform != "darwin":
        print("gfa_run: needs macOS (osascript, screencapture)", file=sys.stderr)
        return 2

    args.share.mkdir(parents=True, exist_ok=True)
    result = args.share / txt_name
    if result.exists():
        result.unlink()
    (args.share / lst_name).write_text(program)

    if not fs_uae_running():
        if not args.config.exists():
            print(f"gfa_run: FS-UAE configuration {args.config} not found (see docs/gfa-original.md)", file=sys.stderr)
            return 2
        start_fs_uae(args.config, args.app)
        time.sleep(args.boot_wait)

    run = subprocess.run(["osascript", "-"], input=script, text=True, capture_output=True)
    if run.returncode != 0:
        print("gfa_run: osascript failed (Accessibility permission?):\n" + run.stderr, file=sys.stderr)
        return 2

    text = wait_for_result(result, args.timeout)
    shot = args.share / f"{name}.png"
    if text is None or args.screenshot:
        subprocess.run(["screencapture", "-x", str(shot)])
    if text is None:
        print(f"gfa_run: no complete result after {args.timeout:.0f} s; screenshot: {shot}", file=sys.stderr)
        return 1
    if not DATEI_RE.search(args.program.read_text()):
        text = strip_end_marker(text)  # test program: exactly what it printed
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text)
    sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
