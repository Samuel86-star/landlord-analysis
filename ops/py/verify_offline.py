#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def build_steps(root):
    wrapper = root / "algorithm" / ("mvnw.cmd" if os.name == "nt" else "mvnw")
    native = root / "algorithm" / "native"
    build = native / "build-release"
    return [
        (
            "Python unit tests",
            [sys.executable, "-m", "unittest", "discover", "-s", "ops/py", "-p", "test_*.py", "-v"],
        ),
        (
            "Python compile check",
            [
                sys.executable,
                "-X",
                "pycache_prefix={}".format(root / "ops" / "py" / "__pycache__"),
                "-m",
                "compileall",
                "-q",
                "ops/py",
            ],
        ),
        ("Java unit tests", [str(wrapper), "test"]),
        ("Java benchmark profile", [str(wrapper), "-Pbenchmark", "test"]),
        (
            "C++ Release configure",
            ["cmake", "-S", str(native), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"],
        ),
        ("C++ Release build", ["cmake", "--build", str(build), "--target", "landlord_test"]),
        ("C++ CTest", ["ctest", "--test-dir", str(build), "--output-on-failure"]),
    ]


def run_steps(steps, root, runner=subprocess.run):
    total = len(steps)
    for index, (name, command) in enumerate(steps, start=1):
        print(
            "[{}/{}] {}: {}".format(
                index, total, name, subprocess.list2cmdline(command)
            ),
            flush=True,
        )
        try:
            result = runner(command, cwd=root)
        except OSError as error:
            print("{} could not start: {}".format(name, error), file=sys.stderr)
            return 127
        if result.returncode != 0:
            print(
                "{} failed with exit code {}".format(name, result.returncode),
                file=sys.stderr,
            )
            return result.returncode
    print("Offline verification passed.")
    return 0


def main():
    return run_steps(build_steps(REPO_ROOT), REPO_ROOT)


if __name__ == "__main__":
    sys.exit(main())
