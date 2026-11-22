"""Auto-indentation rules for GFA-BASIC 3.5 source (pure logic, no GUI).

The block structure follows the parser (src/frontend/parser.cpp):

* openers:  IF, FOR, WHILE, REPEAT, DO, SELECT, PROCEDURE, SUB, FUNCTION
* closers:  ENDIF, NEXT/ENDFOR, WEND/ENDWHILE, UNTIL/ENDREPEAT, LOOP/ENDDO, ENDSELECT,
            ENDFUNC, RETURN/ENDPROC/ENDSUB (RETURN only when a PROCEDURE is the innermost block)
* middles:  ELSE (also ELSE IF) inside IF; CASE, DEFAULT/OTHERWISE inside SELECT.
            As in tests/programs, CASE stands at the level of SELECT.
"""

from __future__ import annotations

INDENT = "  "

OPENERS = {
    "IF": "IF", "FOR": "FOR", "WHILE": "WHILE", "REPEAT": "REPEAT", "DO": "DO",
    "SELECT": "SELECT", "PROCEDURE": "PROC", "FUNCTION": "FUNC", "SUB": "PROC",
}
CLOSERS = {
    "ENDIF": "IF", "NEXT": "FOR", "ENDFOR": "FOR", "WEND": "WHILE", "ENDWHILE": "WHILE",
    "UNTIL": "REPEAT", "ENDREPEAT": "REPEAT", "LOOP": "DO", "ENDDO": "DO",
    "ENDSELECT": "SELECT", "ENDFUNC": "FUNC", "ENDPROC": "PROC", "ENDSUB": "PROC",
}
MIDDLES = {"ELSE": "IF", "CASE": "SELECT", "DEFAULT": "SELECT", "OTHERWISE": "SELECT"}


def _word_at(s: str, start: int = 0) -> tuple[str, int]:
    j = start
    while j < len(s) and (s[j].isalpha() or s[j] == "_"):
        j += 1
    return s[start:j].upper(), j


def split_statements(line: str) -> list[str]:
    """Split a line at ':' into statements; strings and comments are respected.

    A comment (REM, ' or a trailing !) ends the line; DATA and INLINE swallow the rest.
    """
    stmts: list[str] = []
    cur = ""
    in_str = False
    i = 0
    while i < len(line):
        c = line[i]
        if in_str:
            cur += c
            in_str = c != '"'
        elif c == '"':
            in_str = True
            cur += c
        elif c in "'!":
            break
        elif c == ":":
            if cur.strip():
                stmts.append(cur.strip())
            cur = ""
        else:
            cur += c
            word = cur.strip().upper()
            nxt = line[i + 1] if i + 1 < len(line) else " "
            if word in ("DATA", "INLINE", "REM") and not nxt.isalnum():
                if word != "REM":
                    stmts.append((cur + line[i + 1:]).strip())
                cur = ""
                break
        i += 1
    if cur.strip():
        stmts.append(cur.strip())
    return stmts


def _top_level_comma(s: str) -> bool:
    depth = 0
    in_str = False
    for c in s:
        if c == '"':
            in_str = not in_str
        elif in_str:
            continue
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        elif c == "," and depth == 0:
            return True
    return False


class Indenter:
    """Feed lines one after another; ``level_for`` gives the level of each line."""

    def __init__(self, stack: list[str] | None = None) -> None:
        self.stack: list[str] = list(stack or [])

    def level_for(self, line: str) -> int:
        """Indentation level of ``line``; afterwards ``self.stack`` is the state after it."""
        stack = self.stack
        level: int | None = None
        line = line.lstrip()
        if line.startswith(">"):  # GFA editor marker of a folded procedure
            line = line[1:]
        for idx, st in enumerate(split_statements(line)):
            word, end = _word_at(st)
            first = idx == 0
            if word in CLOSERS or (word == "RETURN" and stack and stack[-1] == "PROC"):
                kind = CLOSERS.get(word, "PROC")
                if kind in stack:  # tolerate unbalanced code: close down to the opener
                    while stack.pop() != kind:
                        pass
                if first:
                    level = len(stack)
            elif word in MIDDLES and MIDDLES[word] in stack:
                while stack[-1] != MIDDLES[word]:
                    stack.pop()
                if first:
                    level = len(stack) - 1
            elif word in OPENERS and not (word == "SUB" and _top_level_comma(st[end:])):
                if first:
                    level = len(stack)
                stack.append(OPENERS[word])
        return len(stack) if level is None else level


def indent_text(text: str, indent: str = INDENT) -> str:
    """Re-indent a whole program (leading blanks are replaced, empty lines kept)."""
    ind = Indenter()
    out = []
    for raw in text.split("\n"):
        line = raw.strip()
        if not line:
            out.append("")
            continue
        marker = ""
        if line.startswith(">"):
            marker, line = "> ", line[1:].lstrip()
        out.append(marker + indent * ind.level_for(line) + line)
    return "\n".join(out)


def stack_before(lines: list[str]) -> list[str]:
    """Block stack after the given lines."""
    ind = Indenter()
    for ln in lines:
        ind.level_for(ln)
    return ind.stack


def reindent_line(line: str, level: int, indent: str = INDENT) -> str:
    """``line`` with its leading blanks replaced by ``level`` indentation units. A leading
    '> ' (folded procedure marker of the GFA editor) stays in front."""
    body = line.strip()
    if body.startswith(">"):
        return "> " + indent * level + body[1:].lstrip()
    return indent * level + body


def line_level(before: list[str], line: str) -> int:
    """Level ``line`` belongs to, given the lines above it."""
    return Indenter(stack_before(before)).level_for(line.strip())


def newline_edit(before: list[str], line: str, col: int, indent: str = INDENT) -> tuple[str, str, int]:
    """The user pressed Return at column ``col`` of ``line``.

    Returns (new text of the current line, text of the new line, caret column in the new line).
    The part before the caret is brought to its correct level, the new line starts at the
    level the block structure demands, the rest of the old line moves down."""
    head, tail = line[:col], line[col:]
    ind = Indenter(stack_before(before))
    if head.strip():
        cur = reindent_line(head, ind.level_for(head.strip()), indent)
    else:
        cur = ""
    after = len(ind.stack)
    return cur, indent * after + tail.lstrip(), len(indent) * after


def electric_edit(before: list[str], line: str, indent: str = INDENT) -> str | None:
    """A block word (ENDIF, NEXT, ELSE, CASE ...) was typed: the line with the corrected
    indentation, or None when there is nothing to change."""
    stripped = line.strip()
    word = ""
    for ch in stripped:
        if ch.isalpha():
            word += ch
        else:
            break
    if word.upper() not in set(CLOSERS) | set(MIDDLES) | {"RETURN"} or line.lstrip().startswith(">"):
        return None
    new = reindent_line(line, line_level(before, line), indent)
    return None if new == line else new


def foldable_blocks(lines: list[str]) -> list[tuple[int, int]]:
    """(first, last) line numbers (1-based) of every PROCEDURE/SUB/FUNCTION that is closed, for
    folding in the editor. Blocks that are not closed are not foldable."""
    ind = Indenter()
    blocks: list[tuple[int, int]] = []
    start = None
    for n, ln in enumerate(lines, 1):
        before = list(ind.stack)
        ind.level_for(ln.strip().lstrip(">"))
        after = ind.stack
        if not before and after and after[0] in ("PROC", "FUNC"):
            start = n
        elif before and before[0] in ("PROC", "FUNC") and not after and start is not None:
            blocks.append((start, n))
            start = None
    return blocks
