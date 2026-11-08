#!/usr/bin/env python3
"""Measures the generated code (milestone 0.6, docs/performance.md).

Runs the programs tests/programs/bench_*.lst on z80-test and m68k-test through the test drivers of
the build directory and prints per program and target: instructions, Z80 T-states (the 68000
emulator has no timing) and the size of the program. With --save the numbers become the baseline
(tests/bench/baseline.json), with --compare (the default if a baseline exists) the table shows the
change against it.

    python3 tools/bench.py [--build build] [--save] [--no-compare]
"""
import argparse
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASELINE = os.path.join(ROOT, "tests", "bench", "baseline.json")
LINE = re.compile(r"^(\S+) \((z80-test|m68k-test)\): ok \((\d+) instructions, (?:(\d+) cycles, )?(\d+) bytes\)")


def measure(build):
    programs = sorted(f for f in os.listdir(os.path.join(ROOT, "tests", "programs"))
                      if f.startswith("bench_") and f.endswith(".lst"))
    result = {}
    for driver, var in (("test_z80_programs", "BASIC_Z80_PROGRAM"), ("test_m68k_programs", "BASIC_M68K_PROGRAM")):
        exe = os.path.join(build, "tests", driver)
        for p in programs:
            out = subprocess.run([exe], env=dict(os.environ, **{var: p}), capture_output=True, text=True)
            found = False
            for line in out.stdout.splitlines():
                m = LINE.match(line)
                if m:
                    found = True
                    entry = {"instructions": int(m.group(3)), "bytes": int(m.group(5))}
                    if m.group(4):
                        entry["cycles"] = int(m.group(4))
                    result.setdefault(m.group(1), {})[m.group(2)] = entry
            if not found:
                sys.exit(f"{driver} {p}: no result\n{out.stdout}{out.stderr}")
    return result


def fmt(value, base):
    if base is None or base == 0:
        return f"{value:>12}"
    return f"{value:>12} {100.0 * (value - base) / base:+6.1f}%"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--build", default=os.path.join(ROOT, "build"))
    ap.add_argument("--save", action="store_true", help="store the numbers as the baseline")
    ap.add_argument("--no-compare", action="store_true")
    args = ap.parse_args()
    now = measure(args.build)
    base = {}
    if not args.no_compare and os.path.exists(BASELINE):
        with open(BASELINE) as f:
            base = json.load(f)
    print(f"{'program':<14} {'target':<10} {'instructions':>20} {'cycles':>20} {'bytes':>20}")
    for prog in sorted(now):
        for target in ("z80-test", "m68k-test"):
            e = now[prog].get(target)
            if e is None:
                continue
            b = base.get(prog, {}).get(target, {})
            cyc = fmt(e["cycles"], b.get("cycles")) if "cycles" in e else f"{'-':>12}"
            print(f"{prog:<14} {target:<10} {fmt(e['instructions'], b.get('instructions')):>20} {cyc:>20} "
                  f"{fmt(e['bytes'], b.get('bytes')):>20}")
    if args.save:
        os.makedirs(os.path.dirname(BASELINE), exist_ok=True)
        with open(BASELINE, "w") as f:
            json.dump(now, f, indent=1, sort_keys=True)
            f.write("\n")
        print(f"saved {os.path.relpath(BASELINE, ROOT)}")


if __name__ == "__main__":
    main()
