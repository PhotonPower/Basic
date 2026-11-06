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

    def test_wait(self):
        self.assertEqual(gfa_run.parse_step("wait:1.5", "de"), ["delay 1.5"])

    def test_unknown_step(self):
        with self.assertRaises(ValueError):
            gfa_run.parse_step("click:10,10", "de")

    def test_keys_json_builds(self):
        keys = json.loads((ROOT / "tools" / "gfa" / "keys.json").read_text())
        script = gfa_run.build_script(keys["run"], "de", "FS-UAE", {"file": "dh0:probe1.lst"})
        self.assertTrue(script.startswith('tell application "FS-UAE" to activate'))
        self.assertNotIn("{file}", script)


class Program(unittest.TestCase):
    def test_result_path_is_set(self):
        out = gfa_run.prepare_program('x=1\r\ndatei$="RAM:a.txt"\r\nOPEN "O",#1,datei$\r\n', "DH0:a.txt")
        self.assertIn('datei$="DH0:a.txt"', out)
        self.assertNotIn("\r", out)

    def test_missing_datei_line(self):
        with self.assertRaises(ValueError):
            gfa_run.prepare_program('PRINT "x"\n', "DH0:a.txt")

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
