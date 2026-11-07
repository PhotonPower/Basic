#!/usr/bin/env python3
"""Runs AmigaDOS programs (hunk files made with `basicc --target amiga`) on the emulated Amiga in
FS-UAE and collects what they print. Standard library only; uses the helpers of gfa_run.py.

    python3 tools/gfa/amiga_run.py build/numbers build/strings --out-dir results/

How it works (docs/gfa-original.md):
  1. The programs are copied into the shared directory (DH0: on the Amiga).
  2. FS-UAE is (re)started; the GFA-BASIC disk boots into a shell with the prompt "1>".
  3. For every program `dh0:<name> >dh0:<name>.txt`, the next one only after the result of the
     previous one is complete (a graphics program has an active window of its own).
     The current directory stays the boot disk (the GFA disk has no CD command): a program that
     writes files must name them with DH0:, otherwise they land on the disk image.
  4. The script waits until every result file exists and has not changed for a few seconds.
  5. "1" starts GFA-BASIC again (the boot menu), so that gfa_run.py finds its editor.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

import gfa_run as g


def wait_stable(paths: list[Path], timeout: float, quiet: float) -> bool:
    end = time.time() + timeout
    last: dict[Path, tuple[float, int]] = {}
    stable_since = None
    while time.time() < end:
        state = {p: (p.stat().st_mtime, p.stat().st_size) for p in paths if p.exists()}
        if len(state) == len(paths) and state == last:
            stable_since = stable_since or time.time()
            if time.time() - stable_since >= quiet:
                return True
        else:
            stable_since = None
        last = state
        time.sleep(0.5)
    return False


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("programs", type=Path, nargs="+", help="AmigaDOS executables")
    p.add_argument("--share", type=Path, default=g.DEFAULT_SHARE)
    p.add_argument("--config", type=Path, default=g.DEFAULT_CONFIG)
    p.add_argument("--app", default="FS-UAE")
    p.add_argument("--amiga-layout", choices=sorted(g.LAYOUTS), default=g.DEFAULT_LAYOUT)
    p.add_argument("--boot-wait", type=float, default=25.0)
    p.add_argument("--timeout", type=float, default=300.0)
    p.add_argument("--quiet", type=float, default=6.0, help="seconds without change that count as finished")
    p.add_argument("--out-dir", type=Path, help="copy the results (<name>.txt) here")
    p.add_argument("--after", action="append", default=[], metavar="STEP",
                   help="key step after the start of each program (as in keys.json, e.g. wait:3 type:abc "
                        "key:return), to answer requesters")
    args = p.parse_args(argv)

    names = [g.amiga_name(prog.name) for prog in args.programs]
    results = [args.share / f"{n}.txt" for n in names]
    for prog, name, res in zip(args.programs, names, results):
        shutil.copyfile(prog, args.share / name)
        if res.exists():
            res.unlink()

    if g.fs_uae_running():  # the shell prompt only exists right after booting
        subprocess.run(["osascript", "-e", f'quit app "{args.app}"'])
        time.sleep(3)
    g.start_fs_uae(args.config, args.app)
    time.sleep(args.boot_wait)

    # one program after the other: a program with graphics opens an active window of its own, keys
    # typed ahead would go there instead of to the shell
    ok = True
    for name, res in zip(names, results):
        steps = [f"type:dh0:{name} >dh0:{name}.txt", "key:return", "wait:0.5"] + args.after
        run = subprocess.run(["osascript", "-"], input=g.build_script(steps, args.amiga_layout, args.app, {}),
                             text=True, capture_output=True)
        if run.returncode != 0:
            print("amiga_run: osascript failed or FS-UAE lost the focus:\n" + run.stderr, file=sys.stderr)
            return 3
        ok = wait_stable([res], args.timeout, args.quiet) and ok
    # back to GFA-BASIC for gfa_run.py
    subprocess.run(["osascript", "-"], input=g.build_script(["type:1", "key:return"], args.amiga_layout, args.app, {}),
                   text=True, capture_output=True)
    if not ok:
        shot = args.share / "amiga_run.png"
        subprocess.run(["screencapture", "-x", str(shot)])
        missing = [r.name for r in results if not r.exists()]
        print(f"amiga_run: results incomplete after {args.timeout:.0f} s (missing: {missing}); screenshot: {shot}",
              file=sys.stderr)
        return 1
    for name, res in zip(names, results):
        text = res.read_text(errors="replace")
        if args.out_dir:
            args.out_dir.mkdir(parents=True, exist_ok=True)
            (args.out_dir / f"{name}.txt").write_text(text)
        print(f"==== {name}\n{text}", end="" if text.endswith("\n") else "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
