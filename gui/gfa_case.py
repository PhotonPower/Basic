"""Automatic correction of GFA-BASIC source while typing (pure logic, no GUI).

* ``?`` at the start of a statement becomes ``Print``
* a two letter abbreviation (first letter upper case, second lower case: ``Pr``, ``Fo`` ...)
  from a fixed table is expanded when the word is finished
* every command is written as first letter upper case, the rest lower case (``PRINT`` -> ``Print``)

Strings, comments, DATA lines, labels, procedure names and variables are never touched.
"""

from __future__ import annotations

import re

from gfa_syntax import tokenize_line

# Only pairs that name the command people mean most often. Others (``Re`` = Repeat, Read, Return ...)
# would be guesses and are left out. Own entries: "abbreviations" in the settings file.
ABBREVIATIONS = {
    "Pr": "Print", "Fo": "For", "Ne": "Next", "Wh": "While", "We": "Wend", "Un": "Until",
    "Se": "Select", "Ca": "Case", "El": "Else", "Fu": "Function", "Di": "Dim", "In": "Input",
    "Lo": "Local", "Ex": "Exit If",
}

KEYWORD_TAGS = {"structure", "statement", "function"}
TRIGGERS = set(" :(,;")        # typing one of these finishes the word before it
_WORD = re.compile(r"[A-Za-z_À-￿][\wÀ-￿]*[$%&|!#]?$")


def _in_code(prefix: str) -> bool:
    """True when the end of ``prefix`` is neither inside a string nor inside a comment."""
    in_str = False
    for c in prefix:
        if c == '"':
            in_str = not in_str
        elif not in_str and c in "'!":
            return False
    return not in_str


def at_statement_start(prefix: str) -> bool:
    if not _in_code(prefix):
        return False
    p = prefix.rstrip()
    return p in ("", ">") or p.endswith(":")


def capitalize_word(word: str) -> str:
    return word.capitalize()


def expand_question(line: str, col: int) -> tuple[str, int] | None:
    """``?`` was just typed (it is the character before ``col``): replace it by ``Print``."""
    i = col - 1
    if i < 0 or line[i:i + 1] != "?" or not at_statement_start(line[:i]):
        return None
    rest = line[col:]
    if rest.startswith(" "):
        return line[:i] + "Print" + rest, i + 6
    return line[:i] + "Print " + rest, i + 6


def autocorrect(line: str, col: int, delimiter_typed: bool,
                abbreviations: dict[str, str] | None = None) -> tuple[str, int] | None:
    """Finish the word that ends before the caret.

    ``delimiter_typed``: the character before ``col`` was just typed and ends the word (space, ':',
    '(' ...); otherwise the word ends at ``col`` itself (Return pressed). Returns the new line and
    caret column, or None when nothing changes."""
    end = col - 1 if delimiter_typed else col
    m = _WORD.search(line[:end])
    if not m:
        return None
    start = m.start()
    word = m.group()
    if start > 0 and line[start - 1] in "@.&$%#":
        return None
    rep = None
    for s, e, tag in tokenize_line(line):
        if (s, e) == (start, end) and tag in KEYWORD_TAGS:
            rep = capitalize_word(word)
            break
    if rep is None:
        table = ABBREVIATIONS if abbreviations is None else {**ABBREVIATIONS, **abbreviations}
        if (len(word) == 2 and word[0].isupper() and word[1].islower() and word in table
                and at_statement_start(line[:start])):
            rep = table[word]
    if rep is None or rep == word:
        return None
    return line[:start] + rep + line[end:], col + len(rep) - len(word)


def normalize_line(line: str) -> str:
    """The whole line in the canonical form: statement-start ``?`` -> Print, commands capitalised."""
    out = ""
    for i, c in enumerate(line):
        if c == "?" and at_statement_start(out):
            out += "Print" if line[i + 1:i + 2] == " " else "Print "
        else:
            out += c
    for s, e, tag in reversed(tokenize_line(out)):
        if tag in KEYWORD_TAGS:
            out = out[:s] + capitalize_word(out[s:e]) + out[e:]
    return out


def normalize_text(text: str) -> str:
    return "\n".join(normalize_line(ln) for ln in text.split("\n"))
