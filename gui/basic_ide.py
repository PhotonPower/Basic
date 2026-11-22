#!/usr/bin/env python3
"""Basic IDE - a small editor for GFA-BASIC 3.5 programs with auto-indent, interpreter and compiler.

Runs on Windows, macOS and Linux with plain Python 3.9+ and tkinter (no further packages).
Needs the ``basicc`` executable of this repository (cmake --build build).
"""

from __future__ import annotations

import os
import sys
import tempfile
import tkinter as tk
import tkinter.font as tkfont
from pathlib import Path
from tkinter import filedialog, messagebox, simpledialog, ttk

sys.path.insert(0, str(Path(__file__).resolve().parent))
import runner  # noqa: E402
from gfa_indent import (Indenter, electric_edit, indent_text, newline_edit, reindent_line,  # noqa: E402
                        stack_before)
from gfa_case import TRIGGERS, autocorrect, expand_question, normalize_text  # noqa: E402
from gfa_indent import foldable_blocks  # noqa: E402
from gfa_syntax import tokenize_line  # noqa: E402

APP = "Basic IDE"
AQUA = sys.platform == "darwin"
MOD = "Command" if AQUA else "Control"
MODLABEL = "Cmd" if AQUA else "Strg"

THEMES = {
    False: dict(bg="#ffffff", fg="#1e1e1e", sel="#cfe3ff", cur="#000000", gutter="#f0f0f0", gutterfg="#888888",
                curline="#f6f9ff", out_bg="#fbfbfb", err="#c62828", info="#1565c0",
                structure="#0000cd", statement="#7b1fa2", function="#00695c", string="#b71c1c",
                comment="#6a8759", number="#d35400", label="#ad1457", call="#ad1457", procname="#ad1457"),
    True: dict(bg="#1e1f22", fg="#d4d4d4", sel="#264f78", cur="#ffffff", gutter="#2b2d30", gutterfg="#7d8590",
               curline="#26282c", out_bg="#17181a", err="#ff6b68", info="#6cb6ff",
               structure="#569cd6", statement="#c586c0", function="#4ec9b0", string="#ce9178",
               comment="#6a9955", number="#e9a15f", label="#f48fb1", call="#f48fb1", procname="#f48fb1"),
}


class Document:
    """State of the open file besides the text widget."""

    def __init__(self) -> None:
        self.path: str | None = None
        self.encoding = "utf-8"      # kept when saving (GFA files from the Amiga are Latin-1)
        self.newline = "\n"


class App:
    def __init__(self, root: tk.Tk, files: list[str]) -> None:
        self.root = root
        self.set = runner.load_settings()
        self.doc = Document()
        self.job: runner.Job | None = None
        self.job_kind = ""
        self.job_info: dict = {}
        self.tmpdir = tempfile.TemporaryDirectory(prefix="basic-ide-")
        self.highlight_after: str | None = None
        self.find_win: tk.Toplevel | None = None
        self.text_version = 0                 # counts edits; invalidates the cache of the fold blocks
        self.blocks_cache: tuple[int, dict[int, int]] = (-1, {})
        self.in_autocorrect = False

        root.geometry(self.set.get("geometry", "980x720"))
        root.protocol("WM_DELETE_WINDOW", self.quit)
        self.build_menu()
        self.build_ui()
        self.apply_theme()
        self.bind_keys()
        self.new_file(force=True)
        if files:
            self.open_path(files[0])
        self.poll()

    # ---- UI ---------------------------------------------------------------------------------

    def build_ui(self) -> None:
        self.status = ttk.Label(self.root, anchor="w", padding=(6, 1))
        self.status.pack(fill="x", side="bottom")
        self.paned = ttk.PanedWindow(self.root, orient="vertical")
        self.paned.pack(fill="both", expand=True)

        top = ttk.Frame(self.paned)
        self.gutter = tk.Canvas(top, width=48, highlightthickness=0, bd=0)
        self.gutter.pack(side="left", fill="y")
        self.text = tk.Text(top, undo=True, maxundo=-1, wrap="none", bd=0, highlightthickness=0,
                            padx=6, pady=2, tabs=("1c",), insertwidth=2, exportselection=False)
        ysb = ttk.Scrollbar(top, orient="vertical", command=self.text.yview)
        xsb = ttk.Scrollbar(top, orient="horizontal", command=self.text.xview)
        self.text.configure(yscrollcommand=self.on_yscroll, xscrollcommand=xsb.set)
        self.ysb = ysb
        ysb.pack(side="right", fill="y")
        xsb.pack(side="bottom", fill="x")
        self.text.pack(side="left", fill="both", expand=True)
        self.paned.add(top, weight=3)

        self.out_frame = ttk.Frame(self.paned)
        bar = ttk.Frame(self.out_frame)
        bar.pack(fill="x")
        ttk.Label(bar, text=" Ausgabe").pack(side="left")
        ttk.Button(bar, text="Leeren", width=7, command=self.clear_output).pack(side="right", padx=2, pady=1)
        self.stop_btn = ttk.Button(bar, text="Stopp", width=7, command=self.stop, state="disabled")
        self.stop_btn.pack(side="right", padx=2, pady=1)
        body = ttk.Frame(self.out_frame)
        body.pack(fill="both", expand=True)
        self.out = tk.Text(body, height=10, wrap="word", bd=0, highlightthickness=0, padx=6, pady=2,
                           state="disabled", exportselection=True)
        osb = ttk.Scrollbar(body, orient="vertical", command=self.out.yview)
        self.out.configure(yscrollcommand=osb.set)
        osb.pack(side="right", fill="y")
        self.out.pack(side="left", fill="both", expand=True)
        inbar = ttk.Frame(self.out_frame)
        inbar.pack(fill="x")
        ttk.Label(inbar, text=" Eingabe:").pack(side="left")
        self.entry = ttk.Entry(inbar)
        self.entry.pack(side="left", fill="x", expand=True, padx=4, pady=2)
        self.entry.bind("<Return>", self.send_input)
        if self.set.get("show_output", True):
            self.paned.add(self.out_frame, weight=1)

        self.install_edit_proxy()
        self.text.tag_configure("fold", elide=True)
        self.gutter.bind("<Button-1>", self.on_gutter_click)
        t = self.text
        t.bind("<Return>", self.on_return)
        t.bind("<KP_Enter>", self.on_return)
        t.bind("<Tab>", self.on_tab)
        t.bind("<<PrevWindow>>", self.on_shift_tab)
        t.bind("<ISO_Left_Tab>", self.on_shift_tab)
        t.bind("<KeyRelease>", self.on_key_release)
        t.bind("<<Modified>>", lambda e: self.update_title())
        t.bind("<ButtonRelease-1>", lambda e: self.update_status())
        t.bind("<Configure>", lambda e: self.redraw_gutter())
        t.bind("<MouseWheel>", lambda e: self.root.after_idle(self.redraw_gutter))
        t.bind(f"<{MOD}-a>", self.select_all)

    def apply_theme(self) -> None:
        th = THEMES[bool(self.set["dark"])]
        size = int(self.set["font_size"])
        fam = self.pick_font()
        self.font = tkfont.Font(family=fam, size=size)
        self.text.configure(font=self.font, bg=th["bg"], fg=th["fg"], insertbackground=th["cur"],
                            selectbackground=th["sel"], selectforeground=th["fg"],
                            inactiveselectbackground=th["sel"])
        self.out.configure(font=self.font, bg=th["out_bg"], fg=th["fg"], selectbackground=th["sel"])
        self.gutter.configure(bg=th["gutter"])
        self.gutterfg = th["gutterfg"]
        self.text.tag_configure("curline", background=th["curline"])
        for tag in ("structure", "statement", "function", "string", "comment", "number", "label", "call",
                    "procname"):
            self.text.tag_configure(tag, foreground=th[tag])
        self.bold_font = tkfont.Font(family=fam, size=size, weight="bold")  # keep a reference
        self.text.tag_configure("structure", font=self.bold_font)
        self.text.tag_configure("foldhead", background="#e6eefb" if not self.set["dark"] else "#2f3644")
        self.text.tag_configure("errline", background="#ffd6d6" if not self.set["dark"] else "#5a2a2a")
        self.out.tag_configure("err", foreground=th["err"])
        self.out.tag_configure("info", foreground=th["info"])
        self.out.tag_configure("link", foreground=th["err"], underline=True)
        self.out.tag_configure("input", foreground=th["info"])
        self.text.tag_raise("sel")
        self.root.configure(bg=th["bg"])
        self.rehighlight_all()
        self.redraw_gutter()

    def pick_font(self) -> str:
        have = set(tkfont.families())
        for name in ("Cascadia Mono", "Consolas", "Menlo", "SF Mono", "DejaVu Sans Mono", "Liberation Mono",
                     "Ubuntu Mono", "Courier New"):
            if name in have:
                return name
        return "TkFixedFont" if "TkFixedFont" in have else "Courier"

    def build_menu(self) -> None:
        r = self.root
        m = tk.Menu(r)
        r.config(menu=m)

        def acc(key: str) -> str:
            return f"{MODLABEL}+{key}"

        f = tk.Menu(m, tearoff=False)
        m.add_cascade(label="Datei", menu=f, underline=0)
        f.add_command(label="Neu", accelerator=acc("N"), command=self.new_file)
        f.add_command(label="Öffnen …", accelerator=acc("O"), command=self.open_dialog)
        f.add_command(label="Speichern", accelerator=acc("S"), command=self.save)
        f.add_command(label="Speichern unter …", accelerator=acc("Umschalt+S"), command=self.save_as)
        f.add_separator()
        f.add_command(label="Beenden", accelerator=None if AQUA else "Alt+F4", command=self.quit)

        e = tk.Menu(m, tearoff=False)
        m.add_cascade(label="Bearbeiten", menu=e, underline=0)
        e.add_command(label="Rückgängig", accelerator=acc("Z"), command=self.edit_undo)
        e.add_command(label="Wiederholen", accelerator=acc("Y" if not AQUA else "Umschalt+Z"),
                      command=self.edit_redo)
        e.add_separator()
        e.add_command(label="Ausschneiden", accelerator=acc("X"), command=lambda: self.text.event_generate("<<Cut>>"))
        e.add_command(label="Kopieren", accelerator=acc("C"), command=lambda: self.text.event_generate("<<Copy>>"))
        e.add_command(label="Einfügen", accelerator=acc("V"), command=lambda: self.text.event_generate("<<Paste>>"))
        e.add_command(label="Alles auswählen", accelerator=acc("A"), command=self.select_all)
        e.add_separator()
        e.add_command(label="Suchen / Ersetzen …", accelerator=acc("F"), command=self.show_find)
        e.add_command(label="Weitersuchen", accelerator="F3", command=lambda: self.find_next())
        e.add_command(label="Gehe zu Zeile …", accelerator=acc("G"), command=self.goto_line)
        e.add_separator()
        e.add_command(label="Befehle schreiben wie „Print“ (? ersetzen)", command=self.normalize_all)
        e.add_command(label="Alles neu einrücken", accelerator=acc("Umschalt+I"), command=self.reindent_all)
        e.add_command(label="Auswahl neu einrücken", accelerator=acc("I"), command=self.reindent_selection)

        p = tk.Menu(m, tearoff=False)
        m.add_cascade(label="Programm", menu=p, underline=0)
        p.add_command(label="Im Interpreter starten", accelerator="F5", command=self.run_interpreter)
        p.add_command(label="Stoppen", accelerator="Umschalt+F5", command=self.stop)
        p.add_separator()
        p.add_command(label="Kompilieren (letztes Ziel)", accelerator="F6", command=self.compile_last)
        ct = tk.Menu(p, tearoff=False)
        p.add_cascade(label="Kompilieren für …", menu=ct)
        for t in runner.TARGETS:
            ct.add_command(label=t.label, command=lambda k=t.key: self.compile(k))
        p.add_command(label="Syntax prüfen", accelerator="F7", command=self.check_syntax)

        v = tk.Menu(m, tearoff=False)
        m.add_cascade(label="Ansicht", menu=v, underline=0)
        self.var_dark = tk.BooleanVar(value=bool(self.set["dark"]))
        self.var_auto = tk.BooleanVar(value=bool(self.set["auto_indent"]))
        self.var_out = tk.BooleanVar(value=bool(self.set["show_output"]))
        self.var_case = tk.BooleanVar(value=bool(self.set["auto_case"]))
        v.add_checkbutton(label="Dunkles Farbschema", variable=self.var_dark, command=self.toggle_dark)
        v.add_checkbutton(label="Automatisch einrücken", variable=self.var_auto, command=self.toggle_auto)
        v.add_checkbutton(label="Befehle automatisch korrigieren (?, Pr, PRINT)", variable=self.var_case,
                          command=self.toggle_case)
        v.add_checkbutton(label="Ausgabefenster", variable=self.var_out, command=self.toggle_output)
        v.add_separator()
        v.add_command(label="Alle Prozeduren und Funktionen einklappen", command=lambda: self.fold_all(True))
        v.add_command(label="Alle ausklappen", command=lambda: self.fold_all(False))
        v.add_separator()
        v.add_command(label="Schrift größer", accelerator=acc("+"), command=lambda: self.zoom(1))
        v.add_command(label="Schrift kleiner", accelerator=acc("-"), command=lambda: self.zoom(-1))

        s = tk.Menu(m, tearoff=False)
        m.add_cascade(label="Einstellungen", menu=s, underline=0)
        s.add_command(label="Pfad zu basicc …", command=self.choose_basicc)
        s.add_command(label="CPC-ROM für Snapshots …", command=self.choose_rom)
        s.add_command(label="CPC-Startadresse …", command=self.choose_cpc_org)
        s.add_command(label="Einrückbreite …", command=self.choose_indent)

        h = tk.Menu(m, tearoff=False)
        m.add_cascade(label="Hilfe", menu=h, underline=0)
        h.add_command(label="Über", command=self.about)
        self.menu = m

    def bind_keys(self) -> None:
        r = self.root
        for seq, fn in [
            ("n", self.new_file), ("o", self.open_dialog), ("s", self.save), ("S", self.save_as),
            ("f", self.show_find), ("g", self.goto_line), ("i", self.reindent_selection),
            ("I", self.reindent_all), ("plus", lambda: self.zoom(1)), ("equal", lambda: self.zoom(1)),
            ("minus", lambda: self.zoom(-1)), ("y", self.edit_redo), ("a", self.select_all),
        ]:
            for w in (r, self.text):  # on the text widget too: Tk binds Ctrl-I, Ctrl-O ... itself
                w.bind(f"<{MOD}-{seq}>", lambda e, fn=fn: (fn(), "break")[1])
        for w in (r, self.text):
            w.bind(f"<{MOD}-KP_Add>", lambda e: (self.zoom(1), "break")[1])
            w.bind(f"<{MOD}-KP_Subtract>", lambda e: (self.zoom(-1), "break")[1])
        if AQUA:
            for w in (r, self.text):
                w.bind("<Command-Z>", lambda e: (self.edit_redo(), "break")[1])
        r.bind("<F5>", lambda e: self.run_interpreter())
        r.bind("<Shift-F5>", lambda e: self.stop())
        r.bind("<F6>", lambda e: self.compile_last())
        r.bind("<F7>", lambda e: self.check_syntax())
        r.bind("<F3>", lambda e: self.find_next())

    # ---- editing: auto-indent -----------------------------------------------------------------

    @property
    def unit(self) -> str:
        return " " * int(self.set["indent"])

    def line_text(self, n: int) -> str:
        return self.text.get(f"{n}.0", f"{n}.end")

    def lines_before(self, n: int) -> list[str]:
        return self.text.get("1.0", f"{n}.0 -1c").split("\n") if n > 1 else []

    def set_line_indent(self, n: int, level: int) -> None:
        s = self.line_text(n)
        new = reindent_line(s, level, self.unit)
        if new != s:
            self.text.delete(f"{n}.0", f"{n}.end")
            self.text.insert(f"{n}.0", new)

    def on_return(self, event) -> str | None:
        t = self.text
        if t.tag_ranges("sel"):
            t.delete("sel.first", "sel.last")
        if not self.var_auto.get():
            return None
        n, col = (int(x) for x in t.index("insert").split("."))
        if self.var_case.get():  # a word at the end of the line is finished by Return
            old = self.line_text(n)
            r = autocorrect(old[:col], col, False, self.abbreviations())
            if r is not None:
                self.in_autocorrect = True
                try:
                    self.replace_in_line(n, old, r[0] + old[col:])
                finally:
                    self.in_autocorrect = False
                col = r[1]
        cur, nxt, caret = newline_edit(self.lines_before(n), self.line_text(n), col, self.unit)
        t.edit_separator()
        t.delete(f"{n}.0", f"{n}.end")
        t.insert(f"{n}.0", cur + "\n" + nxt)
        t.mark_set("insert", f"{n + 1}.{caret}")
        t.see("insert")
        t.edit_separator()
        return "break"

    def on_key_release(self, event) -> None:
        # electric dedent: ENDIF, NEXT, ELSE ... jump to their level as soon as the word is typed
        if self.var_auto.get() and (len(event.keysym) == 1 or event.keysym in ("space", "BackSpace", "colon")):
            n, col = (int(x) for x in self.text.index("insert").split("."))
            s = self.line_text(n)
            new = electric_edit(self.lines_before(n), s, self.unit)
            if new is not None:
                self.text.delete(f"{n}.0", f"{n}.end")
                self.text.insert(f"{n}.0", new)
                self.text.mark_set("insert", f"{n}.{max(0, col + len(new) - len(s))}")
        if "fold" in self.text.tag_names("insert"):  # the caret never sits in hidden text
            self.reveal(int(self.text.index("insert").split(".")[0]))
        self.update_status()
        self.highlight_current_line()

    def on_tab(self, event) -> str:
        t = self.text
        if t.tag_ranges("sel"):
            self.shift_selection(+1)
        else:
            col = int(t.index("insert").split(".")[1])
            width = len(self.unit)
            t.insert("insert", " " * (width - col % width))
        return "break"

    def on_shift_tab(self, event) -> str:
        self.shift_selection(-1)
        return "break"

    def selected_lines(self) -> tuple[int, int]:
        t = self.text
        if t.tag_ranges("sel"):
            a = int(t.index("sel.first").split(".")[0])
            end = t.index("sel.last")
            b = int(end.split(".")[0])
            if end.endswith(".0") and b > a:
                b -= 1
            return a, b
        n = int(t.index("insert").split(".")[0])
        return n, n

    def shift_selection(self, direction: int) -> None:
        a, b = self.selected_lines()
        self.text.edit_separator()
        w = len(self.unit)
        for n in range(a, b + 1):
            s = self.line_text(n)
            if direction > 0:
                if s.strip():
                    self.text.insert(f"{n}.0", " " * w)
            else:
                k = min(w, len(s) - len(s.lstrip(" ")))
                if k:
                    self.text.delete(f"{n}.0", f"{n}.{k}")
        self.text.tag_add("sel", f"{a}.0", f"{b}.end")
        self.text.edit_separator()

    def normalize_all(self) -> None:
        t = self.text
        old = t.get("1.0", "end-1c")
        new = normalize_text(old)
        if new == old:
            self.set_status("Alle Befehle sind bereits in der Form „Print“")
            return
        self.fold_all(False)
        pos, yv = t.index("insert"), t.yview()[0]
        t.edit_separator()
        t.delete("1.0", "end")
        t.insert("1.0", new)
        t.edit_separator()
        t.mark_set("insert", pos)
        t.yview_moveto(yv)
        self.set_status("Befehle umgeschrieben")

    def reindent_all(self) -> None:
        t = self.text
        self.fold_all(False)
        old = t.get("1.0", "end-1c")
        new = indent_text(old, self.unit)
        if new == old:
            self.set_status("Einrückung war bereits in Ordnung")
            return
        pos = t.index("insert")
        yv = t.yview()[0]
        t.edit_separator()
        t.delete("1.0", "end")
        t.insert("1.0", new)
        t.edit_separator()
        t.mark_set("insert", pos)
        t.yview_moveto(yv)
        self.after_edit()
        self.set_status("Alles neu eingerückt")

    def reindent_selection(self) -> None:
        a, b = self.selected_lines()
        self.text.edit_separator()
        stack = stack_before(self.lines_before(a))
        ind = Indenter(stack)
        for n in range(a, b + 1):
            s = self.line_text(n).strip()
            if s:
                self.set_line_indent(n, ind.level_for(s.lstrip(">").strip()))
        self.text.edit_separator()
        self.after_edit()

    def select_all(self, event=None) -> str:
        self.text.tag_add("sel", "1.0", "end-1c")
        return "break"

    def edit_undo(self) -> None:
        try:
            self.text.edit_undo()
        except tk.TclError:
            pass
        self.after_edit()

    def edit_redo(self) -> None:
        try:
            self.text.edit_redo()
        except tk.TclError:
            pass
        self.after_edit()

    # ---- highlighting, line numbers, status ------------------------------------------------------

    def after_edit(self) -> None:
        self.update_status()
        self.redraw_gutter()
        self.highlight_current_line()
        if self.highlight_after:
            self.root.after_cancel(self.highlight_after)
        self.highlight_after = self.root.after(120, self.rehighlight_visible)

    def install_edit_proxy(self) -> None:
        """Call after_edit after every insert/delete/undo/redo, whatever its source (keys, mouse
        paste, menu). <<Modified>> only fires on a change of the flag, so it cannot do this."""
        t = self.text
        orig = t._w + "_orig"
        t.tk.call("rename", t._w, orig)
        self.edit_pending = False

        def proxy(*args):
            where = None
            single = (args and args[0] == "insert" and len(args) == 3 and len(str(args[2])) == 1
                      and not self.in_autocorrect and self.var_case.get())
            if single:  # one typed character: remember where it goes
                where = str(t.tk.call(orig, "index", args[1]))
            result = t.tk.call((orig,) + args)
            if args and (args[0] in ("insert", "delete", "replace")
                         or (args[0] == "edit" and len(args) > 1 and args[1] in ("undo", "redo"))):
                self.text_version += 1
                if not self.edit_pending:
                    self.edit_pending = True
                    self.root.after_idle(self.edit_done)
            if where is not None and (str(args[2]) in TRIGGERS or args[2] == "?"):
                line, col = (int(x) for x in where.split("."))
                self.in_autocorrect = True
                try:
                    self.autocorrect_at(line, col + 1, str(args[2]))
                finally:
                    self.in_autocorrect = False
            return result

        t.tk.createcommand(t._w, proxy)

    def abbreviations(self) -> dict:
        ab = self.set.get("abbreviations")
        return ab if isinstance(ab, dict) else {}

    def replace_in_line(self, n: int, old: str, new: str) -> None:
        """Change line n from ``old`` to ``new`` touching only the part that differs."""
        pre = 0
        while pre < min(len(old), len(new)) and old[pre] == new[pre]:
            pre += 1
        suf = 0
        while suf < min(len(old), len(new)) - pre and old[len(old) - 1 - suf] == new[len(new) - 1 - suf]:
            suf += 1
        t = self.text
        t.edit_separator()
        t.delete(f"{n}.{pre}", f"{n}.{len(old) - suf}")
        t.insert(f"{n}.{pre}", new[pre:len(new) - suf])
        t.edit_separator()

    def autocorrect_at(self, n: int, col: int, ch: str) -> None:
        """A character ``ch`` was typed in line n (the caret column after it is ``col``)."""
        old = self.line_text(n)
        r = expand_question(old, col) if ch == "?" else autocorrect(old, col, True, self.abbreviations())
        if r is None:
            return
        new, newcol = r
        caret = self.text.index("insert")
        at_end = caret == f"{n}.{col}"
        self.replace_in_line(n, old, new)
        if at_end:
            self.text.mark_set("insert", f"{n}.{newcol}")

    def edit_done(self) -> None:
        self.edit_pending = False
        self.after_edit()

    def on_yscroll(self, first, last) -> None:
        self.ysb.set(first, last)
        self.redraw_gutter()
        if self.highlight_after:
            self.root.after_cancel(self.highlight_after)
        self.highlight_after = self.root.after(60, self.rehighlight_visible)

    def rehighlight_visible(self) -> None:
        self.highlight_after = None
        first = int(self.text.index("@0,0").split(".")[0])
        last = int(self.text.index(f"@0,{self.text.winfo_height()}").split(".")[0])
        self.highlight_range(max(1, first - 5), last + 5)

    def rehighlight_all(self) -> None:
        self.highlight_range(1, int(self.text.index("end-1c").split(".")[0]))

    def highlight_range(self, a: int, b: int) -> None:
        t = self.text
        b = min(b, int(t.index("end-1c").split(".")[0]))
        tags = ("structure", "statement", "function", "string", "comment", "number", "label", "call", "procname")
        for tag in tags:
            t.tag_remove(tag, f"{a}.0", f"{b}.end")
        for n in range(a, b + 1):
            for s, e, tag in tokenize_line(self.line_text(n)):
                t.tag_add(tag, f"{n}.{s}", f"{n}.{e}")

    def highlight_current_line(self) -> None:
        t = self.text
        t.tag_remove("curline", "1.0", "end")
        if not t.tag_ranges("sel"):
            n = t.index("insert").split(".")[0]
            t.tag_add("curline", f"{n}.0", f"{int(n) + 1}.0")
        t.tag_lower("curline")

    # ---- folding of procedures and functions ------------------------------------------------

    def fold_blocks(self) -> dict[int, int]:
        """first line -> last line of every closed PROCEDURE/FUNCTION (cached per edit)."""
        if self.blocks_cache[0] != self.text_version:
            lines = self.text.get("1.0", "end-1c").split("\n")
            self.blocks_cache = (self.text_version, dict(foldable_blocks(lines)))
        return self.blocks_cache[1]

    def is_folded(self, first: int) -> bool:
        return "fold" in self.text.tag_names(f"{first + 1}.0")

    def set_fold(self, first: int, last: int, fold: bool) -> None:
        a, b = f"{first + 1}.0", f"{last + 1}.0"
        if fold:
            caret = int(self.text.index("insert").split(".")[0])
            if first < caret <= last:
                self.text.mark_set("insert", f"{first}.end")
            self.text.tag_add("fold", a, b)
        else:
            self.text.tag_remove("fold", a, b)

    def fold_all(self, fold: bool) -> None:
        for first, last in self.fold_blocks().items():
            self.set_fold(first, last, fold)
        self.redraw_gutter()

    def reveal(self, line: int) -> None:
        """Unfold the block that hides ``line`` (search hit, error, caret)."""
        for first, last in self.fold_blocks().items():
            if first < line <= last and self.is_folded(first):
                self.set_fold(first, last, False)
        self.redraw_gutter()

    def validate_folds(self, blocks: dict[int, int]) -> None:
        """Drop hidden ranges whose block no longer exists (header or RETURN was edited)."""
        r = [str(x) for x in self.text.tag_ranges("fold")]
        for a, b in zip(r[0::2], r[1::2]):
            first, last = int(a.split(".")[0]) - 1, int(b.split(".")[0]) - 1
            if blocks.get(first) != last:
                self.text.tag_remove("fold", a, b)

    def on_gutter_click(self, event) -> None:
        n = int(self.text.index(f"@0,{event.y}").split(".")[0])
        last = self.fold_blocks().get(n)
        if last is not None:
            self.set_fold(n, last, not self.is_folded(n))
            self.redraw_gutter()

    def redraw_gutter(self) -> None:
        g, t = self.gutter, self.text
        g.delete("all")
        blocks = self.fold_blocks()
        self.validate_folds(blocks)
        total = int(t.index("end-1c").split(".")[0])
        w = max(48, self.font.measure(str(total)) + 16) + 14
        if int(g.cget("width")) != w:
            g.configure(width=w)
        t.tag_remove("foldhead", "1.0", "end")  # folded headers get a tinted background
        for n in blocks:
            if self.is_folded(n):
                t.tag_add("foldhead", f"{n}.0", f"{n}.end")
        t.tag_lower("foldhead")
        first = int(t.index("@0,0").split(".")[0])
        last = int(t.index(f"@0,{t.winfo_height()}").split(".")[0])
        for n in range(first, last + 1):
            d = t.dlineinfo(f"{n}.0")
            if not d:  # hidden by a fold (or below the text)
                continue
            g.create_text(w - 6, d[1], anchor="ne", text=str(n), fill=self.gutterfg, font=self.font)
            if n in blocks:
                cy = d[1] + d[3] / 2
                if self.is_folded(n):   # points right: closed
                    pts = (4, cy - 5, 4, cy + 5, 10, cy)
                else:                   # points down: open
                    pts = (2, cy - 3, 12, cy - 3, 7, cy + 4)
                g.create_polygon(*pts, fill=self.gutterfg, outline="")

    def update_status(self) -> None:
        line, col = self.text.index("insert").split(".")
        enc = self.doc.encoding.upper().replace("-", "")
        self.status.configure(text=f"Zeile {line}, Spalte {int(col) + 1}   |   {enc}   |   "
                                   f"{'CRLF' if self.doc.newline == chr(13) + chr(10) else 'LF'}   |   {self.info_text()}")

    def info_text(self) -> str:
        exe = runner.find_basicc(self.set.get("basicc"))
        return f"basicc: {exe}" if exe else "basicc nicht gefunden (Einstellungen)"

    def set_status(self, msg: str) -> None:
        self.status.configure(text=msg)
        self.root.after(4000, self.update_status)

    def update_title(self) -> None:
        name = Path(self.doc.path).name if self.doc.path else "Unbenannt"
        mark = "• " if self.text.edit_modified() else ""
        self.root.title(f"{mark}{name} – {APP}")

    # ---- files -------------------------------------------------------------------------------

    def confirm_discard(self) -> bool:
        if not self.text.edit_modified():
            return True
        name = Path(self.doc.path).name if self.doc.path else "Unbenannt"
        ans = messagebox.askyesnocancel(APP, f"„{name}“ wurde geändert. Speichern?")
        if ans is None:
            return False
        return self.save() if ans else True

    def new_file(self, force: bool = False) -> None:
        if not force and not self.confirm_discard():
            return
        self.doc = Document()
        self.text.delete("1.0", "end")
        self.text.edit_reset()
        self.text.edit_modified(False)
        self.update_title()
        self.after_edit()

    def open_dialog(self) -> None:
        if not self.confirm_discard():
            return
        p = filedialog.askopenfilename(
            title="Programm öffnen",
            filetypes=[("GFA-BASIC-Quelltext", "*.lst *.LST *.bas *.BAS *.txt"), ("Alle Dateien", "*")])
        if p:
            self.open_path(p, ask=False)

    def open_path(self, path: str, ask: bool = True) -> None:
        if ask and not self.confirm_discard():
            return
        try:
            raw = Path(path).read_bytes()
        except OSError as ex:
            messagebox.showerror(APP, f"Kann „{path}“ nicht lesen:\n{ex}")
            return
        doc = Document()
        doc.path = path
        doc.newline = "\r\n" if b"\r\n" in raw else "\n"
        try:
            s = raw.decode("utf-8")
            doc.encoding = "utf-8"
        except UnicodeDecodeError:
            s = raw.decode("latin-1")
            doc.encoding = "latin-1"
        s = s.replace("\r\n", "\n").replace("\r", "\n")
        self.doc = doc
        self.text.delete("1.0", "end")
        self.text.insert("1.0", s[:-1] if s.endswith("\n") else s)
        self.text.edit_reset()
        self.text.edit_modified(False)
        self.text.mark_set("insert", "1.0")
        self.update_title()
        self.rehighlight_all()
        self.after_edit()

    def save(self) -> bool:
        if not self.doc.path:
            return self.save_as()
        return self.write_to(self.doc.path)

    def save_as(self) -> bool:
        p = filedialog.asksaveasfilename(
            title="Speichern unter", defaultextension=".lst",
            initialfile=Path(self.doc.path).name if self.doc.path else "programm.lst",
            filetypes=[("GFA-BASIC-Quelltext", "*.lst"), ("Alle Dateien", "*")])
        if not p:
            return False
        return self.write_to(p)

    def write_to(self, path: str) -> bool:
        s = self.text.get("1.0", "end-1c") + "\n"
        data = s.replace("\n", self.doc.newline)
        try:
            raw = data.encode(self.doc.encoding)
        except UnicodeEncodeError:
            if not messagebox.askyesno(APP, f"Der Text enthält Zeichen, die in {self.doc.encoding} nicht "
                                            "darstellbar sind.\nStattdessen als UTF-8 speichern?"):
                return False
            self.doc.encoding = "utf-8"
            raw = data.encode("utf-8")
        try:
            Path(path).write_bytes(raw)
        except OSError as ex:
            messagebox.showerror(APP, f"Kann „{path}“ nicht schreiben:\n{ex}")
            return False
        self.doc.path = path
        self.text.edit_modified(False)
        self.update_title()
        self.update_status()
        return True

    def quit(self) -> None:
        if not self.confirm_discard():
            return
        if self.job:
            self.job.kill()
        self.set["geometry"] = self.root.geometry()
        runner.save_settings(self.set)
        self.tmpdir.cleanup()
        self.root.destroy()

    # ---- search ------------------------------------------------------------------------------

    def show_find(self) -> None:
        if self.find_win and self.find_win.winfo_exists():
            self.find_win.lift()
            self.find_entry.focus_set()
            return
        w = self.find_win = tk.Toplevel(self.root)
        w.title("Suchen / Ersetzen")
        w.transient(self.root)
        w.resizable(False, False)
        f = ttk.Frame(w, padding=10)
        f.pack()
        self.find_var, self.repl_var = tk.StringVar(), tk.StringVar()
        self.case_var = tk.BooleanVar(value=False)
        ttk.Label(f, text="Suchen:").grid(row=0, column=0, sticky="w")
        self.find_entry = ttk.Entry(f, textvariable=self.find_var, width=32)
        self.find_entry.grid(row=0, column=1, padx=6, pady=2)
        ttk.Label(f, text="Ersetzen:").grid(row=1, column=0, sticky="w")
        ttk.Entry(f, textvariable=self.repl_var, width=32).grid(row=1, column=1, padx=6, pady=2)
        ttk.Checkbutton(f, text="Groß-/Kleinschreibung", variable=self.case_var).grid(row=2, column=1, sticky="w")
        bb = ttk.Frame(f)
        bb.grid(row=3, column=0, columnspan=2, pady=(8, 0))
        ttk.Button(bb, text="Weitersuchen", command=self.find_next).pack(side="left", padx=2)
        ttk.Button(bb, text="Ersetzen", command=self.replace_one).pack(side="left", padx=2)
        ttk.Button(bb, text="Alle ersetzen", command=self.replace_all).pack(side="left", padx=2)
        w.bind("<Return>", lambda e: self.find_next())
        w.bind("<Escape>", lambda e: w.destroy())
        sel = self.text.get("sel.first", "sel.last") if self.text.tag_ranges("sel") else ""
        if sel and "\n" not in sel:
            self.find_var.set(sel)
        self.find_entry.focus_set()
        self.find_entry.select_range(0, "end")

    def find_next(self) -> bool:
        pat = self.find_var.get() if hasattr(self, "find_var") else ""
        if not pat:
            self.show_find()
            return False
        t = self.text
        start = t.index("sel.last") if t.tag_ranges("sel") else t.index("insert")
        pos = t.search(pat, start, "end", nocase=not self.case_var.get())
        if not pos:
            pos = t.search(pat, "1.0", start, nocase=not self.case_var.get())
        if not pos:
            self.set_status(f"„{pat}“ nicht gefunden")
            return False
        end = f"{pos}+{len(pat)}c"
        self.reveal(int(pos.split(".")[0]))
        t.tag_remove("sel", "1.0", "end")
        t.tag_add("sel", pos, end)
        t.mark_set("insert", end)
        t.see(pos)
        return True

    def replace_one(self) -> None:
        t = self.text
        pat = self.find_var.get()
        if t.tag_ranges("sel") and (t.get("sel.first", "sel.last").lower() == pat.lower()
                                    if not self.case_var.get() else t.get("sel.first", "sel.last") == pat):
            t.edit_separator()
            pos = t.index("sel.first")
            t.delete("sel.first", "sel.last")
            t.insert(pos, self.repl_var.get())
            t.edit_separator()
        self.find_next()

    def replace_all(self) -> None:
        t = self.text
        pat, rep = self.find_var.get(), self.repl_var.get()
        if not pat:
            return
        n, pos = 0, "1.0"
        t.edit_separator()
        while True:
            pos = t.search(pat, pos, "end", nocase=not self.case_var.get())
            if not pos:
                break
            t.delete(pos, f"{pos}+{len(pat)}c")
            t.insert(pos, rep)
            pos = f"{pos}+{len(rep)}c"
            n += 1
        t.edit_separator()
        self.set_status(f"{n} Ersetzung(en)")

    def goto_line(self) -> None:
        total = int(self.text.index("end-1c").split(".")[0])
        n = simpledialog.askinteger("Gehe zu Zeile", f"Zeile (1–{total}):", parent=self.root,
                                    minvalue=1, maxvalue=total)
        if n:
            self.jump_to(n)

    def jump_to(self, line: int, col: int | None = None) -> None:
        t = self.text
        self.reveal(line)
        t.tag_remove("errline", "1.0", "end")
        t.mark_set("insert", f"{line}.{max(0, (col or 1) - 1)}")
        t.tag_add("errline", f"{line}.0", f"{line + 1}.0")
        t.see(f"{line}.0")
        t.focus_set()
        self.root.after(2500, lambda: t.tag_remove("errline", "1.0", "end"))

    # ---- view settings -------------------------------------------------------------------------

    def toggle_dark(self) -> None:
        self.set["dark"] = self.var_dark.get()
        self.apply_theme()

    def toggle_case(self) -> None:
        self.set["auto_case"] = self.var_case.get()

    def toggle_auto(self) -> None:
        self.set["auto_indent"] = self.var_auto.get()

    def toggle_output(self) -> None:
        self.set["show_output"] = self.var_out.get()
        self.show_output(self.var_out.get())

    def show_output(self, show: bool = True) -> None:
        panes = [str(p) for p in self.paned.panes()]
        if show and str(self.out_frame) not in panes:
            self.paned.add(self.out_frame, weight=1)
        elif not show and str(self.out_frame) in panes:
            self.paned.forget(self.out_frame)

    def zoom(self, d: int) -> None:
        self.set["font_size"] = max(7, min(40, int(self.set["font_size"]) + d))
        self.apply_theme()

    def choose_basicc(self) -> None:
        p = filedialog.askopenfilename(title="basicc auswählen",
                                       filetypes=[("basicc", "basicc*"), ("Alle Dateien", "*")])
        if p:
            self.set["basicc"] = p
            runner.save_settings(self.set)
            self.update_status()

    def choose_rom(self) -> None:
        p = filedialog.askopenfilename(title="CPC-6128-ROM (32 KiB) auswählen")
        if p:
            self.set["rom"] = p
            runner.save_settings(self.set)

    def choose_cpc_org(self) -> None:
        text = simpledialog.askstring(
            "CPC-Startadresse",
            "Lade- und Startadresse für CPC-Programme (basicc --org),\n"
            "z. B. &4000; erlaubt &0040 bis &A1FF. Leer = Standard &0100:",
            parent=self.root, initialvalue=self.set.get("cpc_org", ""))
        if text is not None:
            self.set["cpc_org"] = text.strip()
            runner.save_settings(self.set)

    def choose_indent(self) -> None:
        n = simpledialog.askinteger("Einrückbreite", "Leerzeichen je Ebene (1–8):", parent=self.root,
                                    initialvalue=int(self.set["indent"]), minvalue=1, maxvalue=8)
        if n:
            self.set["indent"] = n

    def about(self) -> None:
        messagebox.showinfo(APP, "Editor für GFA-BASIC 3.5 mit automatischer Einrückung.\n\n"
                                 "Interpreter und Compiler: basicc\n"
                                 f"Python {sys.version.split()[0]}, Tk {tk.TkVersion}")

    # ---- running -----------------------------------------------------------------------------

    def need_basicc(self) -> str | None:
        exe = runner.find_basicc(self.set.get("basicc"))
        if exe:
            return exe
        messagebox.showwarning(APP, "Der Compiler „basicc“ wurde nicht gefunden.\n\n"
                                    "Entweder im Repository bauen (cmake --build build) oder unter "
                                    "Einstellungen → Pfad zu basicc auswählen.")
        self.choose_basicc()
        return runner.find_basicc(self.set.get("basicc"))

    def source_for_run(self) -> str | None:
        """File to hand to basicc: the saved file (saved first) or a temporary copy."""
        if self.doc.path:
            if self.text.edit_modified() and not self.save():
                return None
            return self.doc.path
        p = Path(self.tmpdir.name) / "unbenannt.lst"
        p.write_bytes((self.text.get("1.0", "end-1c") + "\n").encode(self.doc.encoding, errors="replace"))
        return str(p)

    def start_job(self, argv: list[str], kind: str, cwd: str | None, **info) -> None:
        self.stop()
        self.show_output(True)
        self.var_out.set(True)
        self.job = runner.Job(argv, cwd=cwd, encoding=self.doc.encoding)
        self.job_kind, self.job_info = kind, info
        try:
            self.job.start()
        except OSError as ex:
            self.append(f"Start fehlgeschlagen: {ex}\n", "err")
            self.job = None
            return
        self.stop_btn.configure(state="normal")
        if kind == "run":
            self.entry.focus_set()

    def run_interpreter(self) -> None:
        exe = self.need_basicc()
        src = exe and self.source_for_run()
        if not src:
            return
        self.clear_output()
        gfx = str(Path(self.tmpdir.name) / "gfx")
        for ext in (".ppm", ".txt"):
            try:
                os.remove(gfx + ext)
            except OSError:
                pass
        self.append(f"▶ Interpreter: {Path(src).name}\n", "info")
        self.start_job([exe, "--run", src, "--gfx", gfx], "run", str(Path(src).parent), gfx=gfx)

    def compile_last(self) -> None:
        self.compile(self.set.get("last_target", "amiga"))

    def compile(self, key: str) -> None:
        t = runner.target_by_key(key)
        exe = self.need_basicc()
        if not exe:
            return
        if t.needs_rom and not (self.set.get("rom") and os.path.isfile(self.set["rom"])):
            messagebox.showinfo(APP, "Für den Snapshot wird das ROM-Abbild der CPC 6128 benötigt.")
            self.choose_rom()
            if not self.set.get("rom"):
                return
        if not self.doc.path and not self.save_as():
            return
        src = self.source_for_run()
        if not src:
            return
        self.set["last_target"] = key
        out = runner.output_path(src, t)
        self.clear_output()
        org = self.set.get("cpc_org", "") if t.target == "cpc" else ""
        self.append(f"⚙ Kompilieren für {t.label}" + (f", Startadresse {org}" if org else "") + "\n", "info")
        self.start_job(runner.compile_argv(exe, src, t, self.set.get("rom"), org), "compile",
                       str(Path(src).parent), out=out)

    def check_syntax(self) -> None:
        exe = self.need_basicc()
        src = exe and self.source_for_run()
        if not src:
            return
        self.clear_output()
        self.append("✔ Syntax prüfen\n", "info")
        self.start_job([exe, "--emit-hir", src], "check", str(Path(src).parent))

    def stop(self) -> None:
        if self.job and not self.job.finished:
            self.job.kill()

    def send_input(self, event=None) -> None:
        s = self.entry.get()
        self.entry.delete(0, "end")
        if self.job and not self.job.finished:
            self.append(s + "\n", "input")
            self.job.send(s + "\n")

    def clear_output(self) -> None:
        self.out.configure(state="normal")
        self.out.delete("1.0", "end")
        self.out.configure(state="disabled")

    def append(self, s: str, tag: str | None = None) -> None:
        o = self.out
        o.configure(state="normal")
        if tag == "err":
            # make 'file:line:col: message' lines clickable
            for part in s.splitlines(True):
                d = runner.parse_diagnostic(part)
                if d:
                    name = f"link{o.index('end')}"
                    o.insert("end", part, ("err", "link", name))
                    o.tag_bind(name, "<Button-1>", lambda e, d=d: self.jump_to(d[0], d[1]))
                    o.tag_bind(name, "<Enter>", lambda e: o.configure(cursor="hand2"))
                    o.tag_bind(name, "<Leave>", lambda e: o.configure(cursor=""))
                    self.jump_to_first_error(d)
                else:
                    o.insert("end", part, "err")
        else:
            o.insert("end", s, tag)
        o.see("end")
        o.configure(state="disabled")

    def jump_to_first_error(self, d) -> None:
        if not self.job_info.get("jumped"):
            self.job_info["jumped"] = True
            self.jump_to(d[0], d[1])

    def poll(self) -> None:
        job = self.job
        if job:
            try:
                for _ in range(200):
                    kind, val = job.events.get_nowait()
                    if kind == "out":
                        if self.job_kind != "check":  # --emit-hir prints the whole program
                            self.append(val)
                    elif kind == "err":
                        self.append(val, "err")
                    else:
                        self.job_done(val)
                        break
            except Exception:  # queue.Empty
                pass
        self.root.after(30, self.poll)

    def job_done(self, code: int) -> None:
        kind, info = self.job_kind, self.job_info
        self.job = None
        self.stop_btn.configure(state="disabled")
        if code == 0:
            if kind == "compile":
                self.append(f"✔ Fertig: {info['out']}\n", "info")
            elif kind == "check":
                self.append("✔ Keine Fehler\n", "info")
            else:
                self.append("■ Programm beendet\n", "info")
                self.show_graphics(info.get("gfx"))
        else:
            self.append(f"✘ Abbruch mit Rückgabewert {code}\n", "err")
            if kind == "run":
                self.show_graphics(info.get("gfx"))

    def show_graphics(self, prefix: str | None) -> None:
        if not prefix or not os.path.isfile(prefix + ".ppm"):
            return
        try:
            img = tk.PhotoImage(file=prefix + ".ppm")
        except tk.TclError:
            return
        w = tk.Toplevel(self.root)
        w.title("Grafik")
        zoom = 2 if img.width() <= 400 else 1
        if zoom > 1:
            img = img.zoom(zoom, zoom)
        lbl = ttk.Label(w, image=img)
        lbl.image = img
        lbl.pack()


def main() -> None:
    root = tk.Tk()
    App(root, [a for a in sys.argv[1:] if not a.startswith("-")])
    root.mainloop()


if __name__ == "__main__":
    main()
