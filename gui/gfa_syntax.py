"""Syntax colouring for GFA-BASIC 3.5 (pure logic: line -> list of (start, end, tag))."""

from __future__ import annotations

import re

from gfa_indent import CLOSERS, MIDDLES, OPENERS

STRUCTURE = set(OPENERS) | set(CLOSERS) | set(MIDDLES) | {"RETURN", "ENDFUNC", "THEN", "CONT", "EXIT", "TO", "DOWNTO", "STEP"}

STATEMENTS = set("""
PRINT LPRINT INPUT LINE LET LOCAL DIM ERASE GOTO GOSUB ON ONERROR RESUME RESTORE READ DATA OPEN CLOSE
WRITE FORM VOID END STOP QUIT CLR DEFFN DEFINT DEFSTR DEFFLT DEFBYT DEFWRD DEFBIT DEFLIST DEFFILL
INC DEC ADD SUB MUL DIV SWAP MID$ OPTION RANDOMIZE CLS COLOR PLOT DRAW BOX PBOX CIRCLE PCIRCLE ELLIPSE
PELLIPSE FILL POLYLINE POLYFILL GRAPHMODE DEFLINE DEFMARK TEXT SETCOLOR SPRITE SOUND WAVE VSYNC PAUSE
DELAY OUT POKE DPOKE LPOKE BMOVE BGET BPUT BLOAD BSAVE GET PUT SGET SPUT FILES CHDIR MKDIR KILL NAME
LPRINT HIDEM SHOWM MENU TITLEW OPENW CLOSEW CLEARW SIZEW MOVEW FULLW ALERT SORT QSORT MAT ARRAYFILL
REM RANDOM FRE FILL PUBLIC SHARED
""".split())

FUNCTIONS = set("""
ABS ACOS ARRPTR ASC ASIN ATN BCHG BCLR BIN$ BSET BTST BYTE CARD CFLOAT CHR$ CINT COS COSQ CRSCOL CRSLIN
CVD CVI CVL CVS DEG DIR$ DPEEK EOF ERR ERR$ EXIST EXP FIX FRAC FRE HEX$ INKEY$ INP INPUT$ INSTR INT
LEFT$ LEN LOC LOF LOG LOG10 LPEEK LPOS MALLOC MAX MFREE MID$ MIN MKD$ MKI$ MKL$ MKS$ MOD MOUSEK MOUSEX
MOUSEY ODD OCT$ PEEK PI POINT POS PRED RAD RAND RANDOM RIGHT$ RINSTR RND ROL ROR ROUND SGN SHL SHR SIN
SINQ SPACE$ SPC SQR STICK STR$ STRIG STRING$ SUCC TAB TAN TIMER TRIM$ TRUE FALSE TRUNC UPPER$ VAL VARPTR
AND OR XOR NOT EQV IMP DIV MOD USING AS OUTPUT APPEND
""".split())

_TOKEN = re.compile(
    r'''(?P<string>"[^"]*"?)
      | (?P<comment>(?:\bREM\b|').*|!.*)
      | (?P<number>(?:&H|\$)[0-9A-Fa-f]+|&O[0-7]+|&X[01]+|%[01]+|\d+\.?\d*(?:[Ee][+-]?\d+)?|\.\d+)
      | (?P<call>@[A-Za-z_À-￿][\wÀ-￿]*)
      | (?P<word>[A-Za-z_À-￿][\wÀ-￿]*[$%&|!#]?)
    ''', re.VERBOSE | re.IGNORECASE)

_LABEL = re.compile(r"^\s*>?\s*([A-Za-z_À-￿][\wÀ-￿]*)\s*:\s*(?:'.*)?$")


def tokenize_line(line: str) -> list[tuple[int, int, str]]:
    """Return (start, end, tag) triples; tags: string comment number call structure statement
    function label procname."""
    out: list[tuple[int, int, str]] = []
    m = _LABEL.match(line)
    if m and m.group(1).upper() not in STRUCTURE | STATEMENTS:
        out.append((m.start(1), m.end(1), "label"))
    stripped = line.lstrip()
    if stripped.upper().startswith(("DATA", "INLINE")):
        # the data is not code
        i = line.upper().find("DATA") if stripped.upper().startswith("DATA") else line.upper().find("INLINE")
        out.append((i, i + (4 if stripped.upper().startswith("DATA") else 6), "statement"))
        return out
    prev_word = ""
    for m in _TOKEN.finditer(line):
        kind = m.lastgroup
        text = m.group()
        if kind == "word":
            up = text.upper()
            if up == "REM":
                out.append((m.start(), len(line), "comment"))
                break
            if prev_word in ("PROCEDURE", "FUNCTION", "SUB", "GOSUB", "DEFFN"):
                tag = "procname"
            elif up in STRUCTURE:
                tag = "structure"
            elif up in STATEMENTS:
                tag = "statement"
            elif up in FUNCTIONS:
                tag = "function"
            else:
                prev_word = ""
                continue
            prev_word = up
            out.append((m.start(), m.end(), tag))
        else:
            prev_word = ""
            out.append((m.start(), m.end(), kind))
    return out
