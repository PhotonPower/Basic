#!/usr/bin/env python3
"""Converts tokenized GFA-BASIC programs (.GFA, "GFA-AMIGAB") into ASCII listings (.LST) with the
original editor in FS-UAE. Standard library only; uses the helpers of gfa_run.py.
    python3 tools/gfa/gfa2lst.py prog1.gfa prog2.gfa --out-dir /some/dir

For every file: the .GFA is copied into the shared directory (DH0:), the editor clears its program
(New), switches to the direct mode (Shift+F9) and runs LOAD "dh0:<name>.gfa" and LIST
"dh0:<name>.lst" (reference: LIST f$ writes the program as ASCII). The listing is copied to
--out-dir. The programs are not changed and nothing is kept in the repository.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

import gfa_run as g


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("programs", type=Path, nargs="+", help="tokenized GFA-BASIC programs (.GFA)")
    p.add_argument("--out-dir", type=Path, required=True, help="where the .LST files go")
    p.add_argument("--share", type=Path, default=g.DEFAULT_SHARE)
    p.add_argument("--config", type=Path, default=g.DEFAULT_CONFIG)
    p.add_argument("--app", default="FS-UAE")
    p.add_argument("--amiga-layout", choices=sorted(g.LAYOUTS), default=g.DEFAULT_LAYOUT)
    p.add_argument("--boot-wait", type=float, default=25.0)
    p.add_argument("--timeout", type=float, default=60.0)
    args = p.parse_args(argv)

    if not g.fs_uae_running():
        g.start_fs_uae(args.config, args.app)
        time.sleep(args.boot_wait)
        boot = g.build_script(["type:1", "key:return", "wait:10"], args.amiga_layout, args.app, {})
        subprocess.run(["osascript", "-"], input=boot, text=True, capture_output=True)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    failed = 0
    for prog in args.programs:
        name = g.amiga_name(prog.stem)
        gfa, lst = args.share / f"{name}.gfa", args.share / f"{name}.lst"
        shutil.copyfile(prog, gfa)
        if lst.exists():
            lst.unlink()
        steps = ["key:return", "wait:0.5", "key:return", "wait:0.5",       # alerts of a previous run
                 "key:shift+f4", "wait:0.7", "key:return", "wait:0.7",     # New, Kill
                 "key:shift+f9", "wait:1.5",                               # direct mode
                 f'type:load "dh0:{name}.gfa"', "key:return", "wait:6",
                 f'type:list "dh0:{name}.lst"', "key:return", "wait:4",
                 "key:escape", "wait:1"]
        script = g.build_script(steps, args.amiga_layout, args.app, {})
        run = subprocess.run(["osascript", "-"], input=script, text=True, capture_output=True)
        if run.returncode != 0:
            print(f"gfa2lst: osascript failed for {prog}:\n{run.stderr}", file=sys.stderr)
            failed += 1
            continue
        end = time.time() + args.timeout
        size = -1
        while time.time() < end:  # until the listing exists and stops growing
            if lst.exists() and lst.stat().st_size == size and size > 0:
                break
            size = lst.stat().st_size if lst.exists() else -1
            time.sleep(1.0)
        if not lst.exists() or lst.stat().st_size == 0:
            shot = args.share / f"{name}.png"
            subprocess.run(["screencapture", "-x", str(shot)])
            print(f"gfa2lst: no listing for {prog}; screenshot: {shot}", file=sys.stderr)
            failed += 1
            continue
        shutil.copyfile(lst, args.out_dir / f"{prog.stem}.LST")
        gfa.unlink()
        print(f"{prog.name} -> {args.out_dir / (prog.stem + '.LST')} ({lst.stat().st_size} bytes)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
