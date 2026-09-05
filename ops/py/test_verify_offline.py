import os
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, os.path.dirname(__file__))

from verify_offline import build_steps, run_steps


class OfflineVerificationTest(unittest.TestCase):
    def test_build_steps_has_the_fixed_order(self):
        root = Path("/repo")
        steps = build_steps(root)

        self.assertEqual(
            [name for name, _ in steps],
            [
                "Python unit tests",
                "Python compile check",
                "Java unit tests",
                "Java benchmark profile",
                "C++ Release configure",
                "C++ Release build",
                "C++ CTest",
            ],
        )
        wrapper = "mvnw.cmd" if os.name == "nt" else "mvnw"
        self.assertEqual(steps[2][1][0], str(root / "algorithm" / wrapper))

    def test_run_steps_executes_in_order(self):
        calls = []

        def fake_run(command, cwd):
            calls.append((command, cwd))
            return SimpleNamespace(returncode=0)

        root = Path("/repo")
        result = run_steps(
            [("first", ["one"]), ("second", ["two"])],
            root,
            runner=fake_run,
        )

        self.assertEqual(result, 0)
        self.assertEqual(calls, [(["one"], root), (["two"], root)])

    def test_run_steps_stops_at_first_failure(self):
        calls = []

        def fake_run(command, cwd):
            calls.append(command)
            return SimpleNamespace(returncode=7 if command == ["two"] else 0)

        result = run_steps(
            [("first", ["one"]), ("second", ["two"]), ("third", ["three"])],
            Path("/repo"),
            runner=fake_run,
        )

        self.assertEqual(result, 7)
        self.assertEqual(calls, [["one"], ["two"]])

    def test_run_steps_reports_missing_tool(self):
        def missing_tool(command, cwd):
            raise FileNotFoundError(command[0])

        result = run_steps(
            [("missing", ["not-installed"])],
            Path("/repo"),
            runner=missing_tool,
        )

        self.assertEqual(result, 127)


if __name__ == "__main__":
    unittest.main()
