import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "gui"))
from gfa_case import autocorrect, expand_question, normalize_line, normalize_text  # noqa: E402
from gfa_indent import foldable_blocks  # noqa: E402

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")


class CaseTest(unittest.TestCase):
    def test_question_mark(self):
        self.assertEqual(expand_question("?", 1), ("Print ", 6))
        self.assertEqual(expand_question("  ? ", 3), ("  Print ", 8))
        self.assertEqual(expand_question("a=1:?", 5), ("a=1:Print ", 10))
        self.assertIsNone(expand_question('x$="?', 5))            # inside a string
        self.assertIsNone(expand_question("a=1 ?", 5))           # not a statement start
        self.assertIsNone(expand_question("' what?", 7))          # comment

    def test_abbreviation(self):
        self.assertEqual(autocorrect("Pr ", 3, True), ("Print ", 6))
        self.assertEqual(autocorrect("  Fo ", 5, True), ("  For ", 6))
        self.assertEqual(autocorrect("Ex ", 3, True), ("Exit If ", 8))
        self.assertEqual(autocorrect("Pr", 2, False), ("Print", 5))          # Return pressed
        self.assertEqual(autocorrect("x=1:Ne ", 7, True), ("x=1:Next ", 9))
        self.assertIsNone(autocorrect("pr ", 3, True))        # must be Capital + lower case
        self.assertIsNone(autocorrect("PR ", 3, True))
        self.assertIsNone(autocorrect("a=Pr ", 5, True))      # not at the start of a statement
        self.assertIsNone(autocorrect("Re ", 3, True))        # ambiguous: not in the table
        self.assertIsNone(autocorrect('Print "Pr ', 11, True))
        self.assertEqual(autocorrect("Qq ", 3, True, {"Qq": "Quit"}), ("Quit ", 5))

    def test_capitalise_while_typing(self):
        self.assertEqual(autocorrect("PRINT ", 6, True), ("Print ", 6))
        self.assertEqual(autocorrect("FOR i%=1 TO ", 12, True), ("FOR i%=1 To ", 12))
        self.assertEqual(autocorrect("  endif", 7, False), ("  Endif", 7))
        self.assertEqual(autocorrect("x$=mid$(", 8, True), ("x$=Mid$(", 8))
        self.assertIsNone(autocorrect("Print ", 6, True))        # already right
        self.assertIsNone(autocorrect("foo ", 4, True))          # variable
        self.assertIsNone(autocorrect("@print ", 7, True))       # procedure call
        self.assertIsNone(autocorrect("PROCEDURE print ", 16, True))  # name of a procedure

    def test_normalize(self):
        self.assertEqual(normalize_line("FOR i%=1 TO 3"), "For i%=1 To 3")
        self.assertEqual(normalize_line('? "A:B";SQR(2) \' PRINT'), 'Print "A:B";Sqr(2) \' PRINT')
        self.assertEqual(normalize_line("DATA 1,PRINT,3"), "Data 1,PRINT,3")
        self.assertEqual(normalize_line("> PROCEDURE foo(a%)"), "> Procedure foo(a%)")
        self.assertEqual(normalize_line("nochmal:"), "nochmal:")
        self.assertEqual(normalize_line("x=1:?x"), "x=1:Print x")
        self.assertEqual(normalize_text("IF a\n?1\nENDIF"), "If a\nPrint 1\nEndif")

    def test_normalize_is_idempotent(self):
        base = os.path.join(ROOT, "tests", "programs")
        for name in sorted(os.listdir(base)):
            if name.endswith(".lst"):
                with open(os.path.join(base, name), encoding="latin-1") as f:
                    once = normalize_text(f.read())
                self.assertEqual(normalize_text(once), once, name)


class FoldTest(unittest.TestCase):
    def test_blocks(self):
        src = ["PRINT 1", "PROCEDURE a", "  IF x", "    PRINT", "  ENDIF", "RETURN", "'", "FUNCTION f(x)",
               "  RETURN 1", "ENDFUNC", "PRINT 2"]
        self.assertEqual(foldable_blocks(src), [(2, 6), (8, 10)])

    def test_open_block_is_not_foldable(self):
        self.assertEqual(foldable_blocks(["PROCEDURE a", "PRINT 1"]), [])

    def test_folded_marker_and_oneliner(self):
        self.assertEqual(foldable_blocks(["> PROCEDURE a", "  PRINT", "RETURN"]), [(1, 3)])
        self.assertEqual(foldable_blocks(["PROCEDURE a : RETURN"]), [])


if __name__ == "__main__":
    unittest.main()
