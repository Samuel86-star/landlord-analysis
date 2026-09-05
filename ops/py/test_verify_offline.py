import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, os.path.dirname(__file__))

from verify_offline import build_steps, run_steps


class OfflineVerificationTest(unittest.TestCase):
    def test_cpp_steps_select_release_configuration(self):
        root = Path("/repo")
        steps = dict(build_steps(root))
        build = str(root / "algorithm" / "native" / "build-release")

        self.assertEqual(
            steps["C++ Release build"],
            ["cmake", "--build", build, "--target", "landlord_test", "--config", "Release"],
        )
        self.assertEqual(
            steps["C++ CTest"],
            ["ctest", "--test-dir", build, "--output-on-failure", "-C", "Release"],
        )

    def test_java_steps_are_offline_and_target_algorithm_pom(self):
        root = Path("/repo")
        steps = dict(build_steps(root))
        pom = str(root / "algorithm" / "pom.xml")
        wrapper = str(root / "algorithm" / ("mvnw.cmd" if os.name == "nt" else "mvnw"))

        self.assertEqual(steps["Java unit tests"], [wrapper, "-o", "-f", pom, "test"])
        self.assertEqual(
            steps["Java benchmark profile"],
            [wrapper, "-o", "-f", pom, "-Pbenchmark", "test"],
        )

    def test_compile_step_overrides_unwritable_inherited_cache(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source_dir = root / "ops" / "py"
            source_dir.mkdir(parents=True)
            (source_dir / "probe.py").write_text("value = 1\n", encoding="utf-8")
            inherited_prefix = root / "unwritable-cache"
            inherited_prefix.touch()

            compile_command = next(
                command
                for name, command in build_steps(root)
                if name == "Python compile check"
            )
            local_prefix = root / "ops" / "py" / "__pycache__"
            environment = os.environ.copy()
            environment["PYTHONPYCACHEPREFIX"] = str(inherited_prefix)
            result = subprocess.run(
                compile_command,
                cwd=root,
                env=environment,
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                compile_command[1:3],
                ["-X", "pycache_prefix={}".format(local_prefix)],
            )
            self.assertTrue(list(local_prefix.rglob("*.pyc")))

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
