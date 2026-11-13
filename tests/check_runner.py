"""Regression: --suite all must fail if the delivered example is absent."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = (root / "build").resolve()
    build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="runner-regression-", dir=build) as temporary:
        # The context manager only deletes this newly created, verified build child.
        isolated = Path(temporary).resolve()
        if isolated.parent != build:
            raise RuntimeError("Unexpected temporary directory")
        copied = isolated / "component"
        shutil.copytree(root, copied, ignore=shutil.ignore_patterns("build", "__pycache__", "host_demo.c"))
        result = subprocess.run([sys.executable, str(copied / "tests/run.py"), "--cc", args.cc,
                                 "--suite", "all", "--opt", "0"],
                                capture_output=True, text=True, timeout=120)
        if result.returncode == 0:
            print("FAIL: --suite all reported PASS without examples/host_demo.c")
            return 1
        if "MISSING REQUIRED EXAMPLE" not in result.stderr:
            print(result.stdout, result.stderr)
            print("FAIL: missing example was not the reported cause")
            return 1
    print("PASS: --suite all rejects a package missing host_demo.c")
    return 0


if __name__ == "__main__":
    sys.exit(main())
