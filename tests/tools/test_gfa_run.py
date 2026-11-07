"""Tests for tools/gfa/gfa_run.py that need neither macOS nor FS-UAE."""

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "gfa"))
import gfa_run  # noqa: E402


class KeyEvents(unittest.TestCase):
    def test_letters_us(self):
        self.assertEqual(gfa_run.key_events("ab", "us"), [(0, False), (11, False)])

    def test_y_z_swapped_on_german_keymap(self):
        # 'z' on a German Amiga is the key at the US 'y' position and vice versa
        self.assertEqual(gfa_run.key_events("z", "de"), [(16, False)])
        self.assertEqual(gfa_run.key_events("y", "de"), [(6, False)])
        self.assertEqual(gfa_run.key_events("z", "us"), [(6, False)])

    def test_colon(self):
        self.assertEqual(gfa_run.key_events(":", "de"), [(47, True)])  # shift + '.'
        self.assertEqual(gfa_run.key_events(":", "us"), [(41, True)])  # shift + ';'

    def test_upper_case_uses_shift(self):
        self.assertEqual(gfa_run.key_events("A", "us"), [(0, True)])

    def test_unsupported_character(self):
        with self.assertRaises(ValueError):
            gfa_run.key_events('"', "de")

    def test_path_is_typeable(self):
        for layout in ("de", "us"):
            self.assertEqual(len(gfa_run.key_events("dh0:probe1.lst", layout)), 14)


class Steps(unittest.TestCase):
    def test_named_key_with_modifier(self):
        lines = gfa_run.parse_step("key:shift+f10", "de")
        self.assertIn("key code 109 using {shift down}", lines[0])

    def test_key_repeat(self):
        lines = gfa_run.parse_step("key:delete*3", "us")
        self.assertEqual(sum("key code 117" in line for line in lines), 3)

    def test_default_layout_is_us(self):
        # The GFA-BASIC 3.5 disk boots with the US keymap (checked on the original: ':' typed
        # with the German table arrived as '>').
        self.assertEqual(gfa_run.DEFAULT_LAYOUT, "us")

    def test_wait(self):
        self.assertEqual(gfa_run.parse_step("wait:1.5", "de"), ["delay 1.5"])

    def test_unknown_step(self):
        with self.assertRaises(ValueError):
            gfa_run.parse_step("click:10,10", "de")

    def test_script_waits_for_focus(self):
        # FS-UAE must be frontmost before keys are sent (two runs lost their keys without this)
        script = gfa_run.build_script(["key:return"], "us", "FS-UAE", {})
        self.assertIn("frontmost is true", script)
        self.assertLess(script.index("frontmost is true"), script.index("key code 36"))

    def test_keys_json_builds(self):
        keys = json.loads((ROOT / "tools" / "gfa" / "keys.json").read_text())
        script = gfa_run.build_script(keys["run"], "de", "FS-UAE", {"file": "dh0:probe1.lst"})
        self.assertTrue(script.startswith("repeat"))
        self.assertIn('tell application "FS-UAE" to activate', script)
        self.assertNotIn("{file}", script)


class Program(unittest.TestCase):
    def test_result_path_is_set(self):
        out = gfa_run.prepare_program('x=1\r\ndatei$="RAM:a.txt"\r\nOPEN "O",#1,datei$\r\n', "DH0:a.txt")
        self.assertIn('datei$="DH0:a.txt"', out)
        self.assertNotIn("\r", out)

    def test_screen_program_is_redirected(self):
        # a program without datei$ prints to the screen; it is rewritten to print into the file
        src = 'x=1\nPRINT "a";x\n  print\nPRINT USING "##",x\nEND\nPROCEDURE p\nPRINT 2\nRETURN\n'
        out = gfa_run.prepare_program(src, "DH0:a.txt")
        lines = out.splitlines()
        self.assertEqual(lines[0], 'zz.out$="DH0:a.txt"')
        self.assertEqual(lines[1], 'OPEN "O",#1,zz.out$')
        self.assertIn('PRINT #1,"a";x', lines)
        self.assertIn('  PRINT #1', lines)
        self.assertIn('PRINT #1,USING "##",x', lines)
        self.assertNotIn("END", lines)
        self.assertEqual(lines.count("@zz.end"), 2)  # for END and before the first PROCEDURE
        self.assertIn("PRINT #1,2", lines)
        self.assertEqual(lines[-5:], ["PROCEDURE zz.end", '  PRINT #1,"ende"', "  CLOSE #1", "  END", "RETURN"])

    def test_error_routine_reopens_the_file(self):
        src = "ON ERROR GOSUB f\nERROR 5\nEND\nPROCEDURE f\nPRINT ERR\nRESUME NEXT\nRETURN\n"
        lines = gfa_run.prepare_program(src, "DH0:a.txt").splitlines()
        i = lines.index("PROCEDURE f")
        self.assertEqual(lines[i + 1:i + 4], ["  zz.err%=ERR", "  CLOSE #1", '  OPEN "A",#1,zz.out$'])

    def test_result_without_end_marker(self):
        self.assertEqual(gfa_run.strip_end_marker("a\nb\nende\n"), "a\nb\n")

    def test_non_ascii_rejected(self):
        with self.assertRaises(ValueError):
            gfa_run.prepare_program('datei$="a"\nPRINT "Grüße"\n', "DH0:a.txt")

    def test_probe_programs_prepare(self):
        for lst in (ROOT / "tests" / "probes").glob("*.lst"):
            with self.subTest(lst.name):
                out = gfa_run.prepare_program(lst.read_text(), f"DH0:{gfa_run.amiga_name(lst.stem)}.txt")
                self.assertIn("ende", out)

    def test_amiga_name(self):
        self.assertEqual(gfa_run.amiga_name("Probe_1"), "probe1")


if __name__ == "__main__":
    unittest.main()
