import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "gui"))
from gfa_indent import Indenter, indent_text, split_statements, stack_before  # noqa: E402

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")


class IndentTest(unittest.TestCase):
    def test_blocks(self):
        src = "FOR i%=1 TO 3\nIF i%=2\nPRINT i%\nELSE\nPRINT 0\nENDIF\nNEXT i%"
        want = "FOR i%=1 TO 3\n  IF i%=2\n    PRINT i%\n  ELSE\n    PRINT 0\n  ENDIF\nNEXT i%"
        self.assertEqual(indent_text(src), want)

    def test_select(self):
        src = "SELECT a%\nCASE 1\nPRINT 1\nDEFAULT\nPRINT 2\nENDSELECT"
        want = "SELECT a%\nCASE 1\n  PRINT 1\nDEFAULT\n  PRINT 2\nENDSELECT"
        self.assertEqual(indent_text(src), want)

    def test_return_in_function_keeps_block(self):
        src = "FUNCTION f(x%)\nIF x%\nRETURN 1\nENDIF\nRETURN 2\nENDFUNC\nPRINT 1"
        want = "FUNCTION f(x%)\n  IF x%\n    RETURN 1\n  ENDIF\n  RETURN 2\nENDFUNC\nPRINT 1"
        self.assertEqual(indent_text(src), want)

    def test_return_closes_procedure(self):
        self.assertEqual(indent_text("PROCEDURE p\nPRINT 1\nRETURN\nPRINT 2"),
                         "PROCEDURE p\n  PRINT 1\nRETURN\nPRINT 2")

    def test_colon_and_comments(self):
        self.assertEqual(split_statements('PRINT "a:b" : x=1 \' c: d'), ['PRINT "a:b"', "x=1"])
        self.assertEqual(split_statements("DATA 1,2:3"), ["DATA 1,2:3"])
        self.assertEqual(indent_text("IF a : b=1 : ENDIF\nPRINT 1"), "IF a : b=1 : ENDIF\nPRINT 1")
        self.assertEqual(indent_text("REM FOR x\nPRINT 1"), "REM FOR x\nPRINT 1")

    def test_sub_command_is_no_block(self):
        self.assertEqual(indent_text("SUB a%,1\nPRINT a%"), "SUB a%,1\nPRINT a%")
        self.assertEqual(indent_text("SUB p(a,b)\nPRINT a\nRETURN"), "SUB p(a,b)\n  PRINT a\nRETURN")

    def test_unbalanced_does_not_crash(self):
        self.assertEqual(indent_text("NEXT\nENDIF\nELSE\nCASE 1\nPRINT 1"), "NEXT\nENDIF\nELSE\nCASE 1\nPRINT 1")

    def test_stack_before(self):
        self.assertEqual(stack_before(["FOR i=1 TO 2", "IF a"]), ["FOR", "IF"])

    def test_repo_programs_are_stable(self):
        """The programs of the repository are already indented the way the rules say."""
        base = os.path.join(ROOT, "tests", "programs")
        checked = 0
        for name in sorted(os.listdir(base)):
            if not name.endswith(".lst"):
                continue
            with open(os.path.join(base, name), encoding="latin-1") as f:
                text = f.read().replace("\r\n", "\n").rstrip("\n")
            if "INLINE" in text:
                continue
            got = indent_text(text)
            bad = [(a, b) for a, b in zip(text.split("\n"), got.split("\n")) if a != b]
            self.assertEqual(bad[:3], [], name)
            checked += 1
        self.assertGreater(checked, 10)


class EditTest(unittest.TestCase):
    def test_return_after_opener(self):
        from gfa_indent import newline_edit
        self.assertEqual(newline_edit([], "FOR i%=1 TO 3", 13), ("FOR i%=1 TO 3", "  ", 2))
        self.assertEqual(newline_edit(["FOR i=1 TO 3"], "  IF a", 6), ("  IF a", "    ", 4))

    def test_return_after_closer(self):
        from gfa_indent import newline_edit
        before = ["FOR i=1 TO 3", "  IF a", "    PRINT 1"]
        self.assertEqual(newline_edit(before, "      ENDIF", 11), ("  ENDIF", "  ", 2))

    def test_return_in_middle_moves_rest(self):
        from gfa_indent import newline_edit
        self.assertEqual(newline_edit(["IF a"], "  PRINT 1:PRINT 2", 10), ("  PRINT 1:", "  PRINT 2", 2))

    def test_return_on_blank_line_cleans_trailing_blanks(self):
        from gfa_indent import newline_edit
        self.assertEqual(newline_edit(["IF a"], "  ", 2), ("", "  ", 2))

    def test_return_keeps_folded_marker(self):
        from gfa_indent import newline_edit
        self.assertEqual(newline_edit([], "> PROCEDURE p", 13), ("> PROCEDURE p", "  ", 2))

    def test_electric(self):
        from gfa_indent import electric_edit
        before = ["FOR i=1 TO 3", "  IF a", "    PRINT 1"]
        self.assertEqual(electric_edit(before, "    ENDIF"), "  ENDIF")
        self.assertEqual(electric_edit(before, "    ELSE"), "  ELSE")
        self.assertIsNone(electric_edit(before, "  ENDIF"))
        self.assertIsNone(electric_edit(before, "    PRINT 2"))       # no block word
        self.assertIsNone(electric_edit(before, "    ENDIFFY=1"))    # a variable, not a keyword
        self.assertEqual(electric_edit(["FUNCTION f", "  IF a"], "    RETURN 1"), None)
        self.assertEqual(electric_edit(["PROCEDURE p", "  PRINT 1"], "  RETURN"), "RETURN")


if __name__ == "__main__":
    unittest.main()
