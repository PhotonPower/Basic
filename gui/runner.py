"""Starting basicc (interpreter, compiler) as a child process; independent of the GUI toolkit."""

from __future__ import annotations

import codecs
import json
import os
import queue
import re
import shutil
import subprocess
import sys
import threading
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
EXE = "basicc.exe" if os.name == "nt" else "basicc"


# ---- finding the compiler -------------------------------------------------------------------

def find_basicc(configured: str | None = None) -> str | None:
    """Search order: setting, $BASICC, build directories of this repository, PATH."""
    candidates: list[Path] = []
    if configured:
        candidates.append(Path(configured).expanduser())
    if os.environ.get("BASICC"):
        candidates.append(Path(os.environ["BASICC"]))
    root = HERE.parent
    for sub in ("build", "build/Release", "build/Debug", "build/RelWithDebInfo", "bin", ""):
        candidates.append(root / sub / EXE)
    candidates.append(HERE / EXE)
    for c in candidates:
        if c.is_file() and os.access(c, os.X_OK):
            return str(c)
    return shutil.which("basicc")


# ---- compile targets -------------------------------------------------------------------------

@dataclass(frozen=True)
class Target:
    key: str
    label: str
    target: str          # name for basicc --target
    suffix: str          # ending of the output file ("" = none)
    needs_rom: bool = False


TARGETS = [
    Target("amiga", "Amiga (AmigaDOS-Programm)", "amiga", ""),
    Target("amiga-s", "Amiga (Assembler-Quelltext .s)", "amiga", ".s"),
    Target("cpc", "Amstrad CPC (Binärdatei .bin)", "cpc", ".bin"),
    Target("cpc-sna", "Amstrad CPC (Snapshot .sna, braucht ROM)", "cpc", ".sna", needs_rom=True),
    Target("cpc-s", "Amstrad CPC (Assembler-Quelltext .s)", "cpc", ".s"),
    Target("pc", "PC (C++-Quelltext .cpp)", "pc", ".cpp"),
    Target("z80-test", "Z80-Testplattform (.s)", "z80-test", ".s"),
    Target("m68k-test", "MC68000-Testplattform (.s)", "m68k-test", ".s"),
]


def target_by_key(key: str) -> Target:
    for t in TARGETS:
        if t.key == key:
            return t
    return TARGETS[0]


def output_path(source: str, t: Target) -> str:
    """Output next to the source: prog.lst -> prog, prog.bin, prog.cpp, ..."""
    stem = str(Path(source).with_suffix(""))
    return stem + t.suffix if t.suffix else stem


def compile_argv(basicc: str, source: str, t: Target, rom: str | None = None) -> list[str]:
    argv = [basicc, "--target", t.target, source, "-o", output_path(source, t)]
    if t.needs_rom and rom:
        argv += ["--rom", rom]
    return argv


# ---- diagnostics ----------------------------------------------------------------------------

_DIAG = re.compile(r"^(?P<file>.+?):(?P<line>\d+)(?::(?P<col>\d+))?: (?P<msg>.*)$")


def parse_diagnostic(text: str) -> tuple[int, int | None, str] | None:
    """'file:line:col: message' or 'file:line: runtime error ...' -> (line, col, message)."""
    m = _DIAG.match(text.rstrip("\r\n"))
    if not m:
        return None
    col = m.group("col")
    return int(m.group("line")), int(col) if col else None, m.group("msg")


# ---- process --------------------------------------------------------------------------------

@dataclass
class Job:
    """A running child process. Output arrives through ``events`` as ('out'|'err', text) and a
    final ('exit', returncode); the GUI polls the queue from its own thread."""

    argv: list[str]
    cwd: str | None = None
    encoding: str = "utf-8"
    events: queue.Queue = field(default_factory=queue.Queue)
    proc: subprocess.Popen | None = None
    finished: bool = False

    def start(self) -> None:
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0) if os.name == "nt" else 0
        self.proc = subprocess.Popen(
            self.argv, cwd=self.cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, creationflags=flags)
        threads = [threading.Thread(target=self._pump, args=(self.proc.stdout, "out"), daemon=True),
                   threading.Thread(target=self._pump, args=(self.proc.stderr, "err"), daemon=True)]
        for t in threads:
            t.start()
        threading.Thread(target=self._wait, args=(threads,), daemon=True).start()

    def _pump(self, stream, kind: str) -> None:
        dec = codecs.getincrementaldecoder(self.encoding)(errors="replace")
        held = ""  # a CR at the end of a chunk: LF may follow in the next one (Windows: CRLF)
        while True:
            chunk = os.read(stream.fileno(), 4096)  # returns as soon as something is there
            if not chunk:
                break
            text = held + dec.decode(chunk)
            held = "\r" if text.endswith("\r") else ""
            if held:
                text = text[:-1]
            text = text.replace("\r\n", "\n")
            if text:
                self.events.put((kind, text))
        tail = (held + dec.decode(b"", final=True)).replace("\r\n", "\n")
        if tail:
            self.events.put((kind, tail))

    def _wait(self, threads) -> None:
        assert self.proc is not None
        code = self.proc.wait()
        for t in threads:
            t.join()
        self.finished = True
        for stream in (self.proc.stdin, self.proc.stdout, self.proc.stderr):
            try:
                if stream:
                    stream.close()
            except OSError:
                pass
        self.events.put(("exit", code))

    def send(self, text: str) -> None:
        if self.proc and self.proc.stdin and not self.finished:
            try:
                self.proc.stdin.write(text.encode(self.encoding, errors="replace"))
                self.proc.stdin.flush()
            except (BrokenPipeError, OSError, ValueError):
                pass

    def close_input(self) -> None:
        if self.proc and self.proc.stdin:
            try:
                self.proc.stdin.close()
            except OSError:
                pass

    def kill(self) -> None:
        if self.proc and self.proc.poll() is None:
            self.proc.kill()


# ---- settings -------------------------------------------------------------------------------

def config_path() -> Path:
    if os.name == "nt":
        base = Path(os.environ.get("APPDATA", Path.home() / "AppData" / "Roaming"))
    elif sys.platform == "darwin":
        base = Path.home() / "Library" / "Application Support"
    else:
        base = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    return base / "basic-ide" / "config.json"


DEFAULTS = {
    "basicc": "", "rom": "", "font_size": 12, "indent": 2, "auto_indent": True, "dark": False,
    "last_target": "amiga", "geometry": "980x720", "show_output": True,
}


def load_settings(path: Path | None = None) -> dict:
    data = dict(DEFAULTS)
    try:
        data.update(json.loads((path or config_path()).read_text(encoding="utf-8")))
    except (OSError, ValueError):
        pass
    return data


def save_settings(data: dict, path: Path | None = None) -> None:
    p = path or config_path()
    try:
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(data, indent=2), encoding="utf-8")
    except OSError:
        pass
