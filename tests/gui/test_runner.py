import os
import sys
import tempfile
import time
import unittest
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "gui"))
import runner  # noqa: E402


def collect(job, timeout=20):
    out, end = {"out": "", "err": ""}, time.time() + timeout
    while time.time() < end:
        kind, val = job.events.get(timeout=timeout)
        if kind == "exit":
            return out, val
        out[kind] += val
    raise TimeoutError


class RunnerTest(unittest.TestCase):
    def test_diagnostics(self):
        self.assertEqual(runner.parse_diagnostic("e.lst:2:4: variable expected"), (2, 4, "variable expected"))
        self.assertEqual(runner.parse_diagnostic("e.lst:2: runtime error 26: end of file"),
                         (2, None, "runtime error 26: end of file"))
        self.assertEqual(runner.parse_diagnostic("C:\\x\\e.lst:7:1: boom"), (7, 1, "boom"))
        self.assertIsNone(runner.parse_diagnostic("hello"))

    def test_output_paths(self):
        t = runner.target_by_key
        base = os.path.join(os.sep, "a", "p")  # separators differ between Windows and the rest
        self.assertEqual(runner.output_path(base + ".lst", t("amiga")), base)
        self.assertEqual(runner.output_path(base + ".lst", t("cpc")), base + ".bin")
        self.assertEqual(runner.output_path(base + ".lst", t("pc")), base + ".cpp")
        argv = runner.compile_argv("bc", "/a/p.lst", t("cpc-sna"), "rom.bin")
        self.assertEqual(argv[-2:], ["--rom", "rom.bin"])

    def test_settings_roundtrip(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "x" / "c.json"
            self.assertEqual(runner.load_settings(p), runner.DEFAULTS)
            runner.save_settings({"font_size": 20}, p)
            self.assertEqual(runner.load_settings(p)["font_size"], 20)

    def test_job_roundtrip_with_input(self):
        # echo a line via the python interpreter: output and stdin both work, exit code arrives
        job = runner.Job([sys.executable, "-c", "print(input())"])
        job.start()
        job.send("hallo\n")
        out, code = collect(job)
        self.assertEqual((out["out"].strip(), code), ("hallo", 0))

    def test_crlf_is_normalised(self):
        code = "import sys; sys.stdout.buffer.write(b'a\\r\\nb\\r'); sys.stdout.flush(); " \
               "import time; time.sleep(0.3); sys.stdout.buffer.write(b'\\nc\\r\\n')"
        job = runner.Job([sys.executable, "-c", code])
        job.start()
        out, _ = collect(job)
        self.assertEqual(out["out"], "a\nb\nc\n")

    def test_job_kill(self):
        job = runner.Job([sys.executable, "-c", "import time; time.sleep(60)"])
        job.start()
        job.kill()
        _, code = collect(job)
        self.assertNotEqual(code, 0)

    def test_with_real_basicc(self):
        exe = runner.find_basicc()
        if not exe:
            self.skipTest("basicc not built")
        with tempfile.TemporaryDirectory() as d:
            src = Path(d) / "t.lst"
            src.write_text('PRINT "Name";\nINPUT n$\nPRINT "Hallo ";n$\n', encoding="latin-1")
            job = runner.Job([exe, "--run", str(src)], cwd=d, encoding="latin-1")
            job.start()
            kind, first = job.events.get(timeout=20)   # the prompt must arrive before any input
            self.assertEqual((kind, first), ("out", "Name? "))
            job.send("Welt\n")
            out, code = collect(job)
            self.assertEqual((out["out"], code), ("Hallo Welt\n", 0))
            bad = Path(d) / "b.lst"
            bad.write_text("PRINT 1\nFOR\n")
            job = runner.Job([exe, "--run", str(bad)])
            job.start()
            out, code = collect(job)
            self.assertEqual(code, 1)
            self.assertEqual(runner.parse_diagnostic(out["err"])[0], 2)
            job = runner.Job(runner.compile_argv(exe, str(src), runner.target_by_key("pc")))
            job.start()
            _, code = collect(job)
            self.assertEqual(code, 0)
            self.assertTrue((Path(d) / "t.cpp").is_file())


    def test_normalised_programs_mean_the_same(self):
        """Capitalising commands and replacing '?' must not change what the compiler sees."""
        import subprocess
        from gfa_case import normalize_text
        exe = runner.find_basicc()
        if not exe:
            self.skipTest("basicc not built")
        base = Path(__file__).resolve().parents[2] / "tests" / "programs"
        checked = 0
        with tempfile.TemporaryDirectory() as d:
            for src in sorted(base.glob("*.lst")):
                raw = src.read_bytes().decode("latin-1")
                norm = Path(d) / "n.lst"
                norm.write_bytes(normalize_text(raw).encode("latin-1"))
                a = subprocess.run([exe, "--emit-hir", str(src)], capture_output=True)
                b = subprocess.run([exe, "--emit-hir", str(norm)], capture_output=True)
                self.assertEqual((a.returncode, a.stdout), (b.returncode, b.stdout), src.name)
                checked += 1
        self.assertGreater(checked, 30)


if __name__ == "__main__":
    unittest.main()
