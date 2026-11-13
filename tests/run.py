"""C99 host tests. Each case loads the real driver in a fresh child process."""
import argparse
import ctypes
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEVICE = ROOT / "User/Device/ILI9488"


def invoke(command, **kwargs):
    return subprocess.run([str(x) for x in command], check=True, timeout=60, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc")
    parser.add_argument("--suite", choices=["api", "transport", "pixels", "lifecycle", "boardport", "candidate", "graphics", "all"], default="all")
    parser.add_argument("--opt", choices=["0", "2", "all"], default="all")
    parser.add_argument("--units", type=int, default=64)
    parser.add_argument("--arch", choices=["native", "32", "64"], default="native")
    parser.add_argument("--ubsan-trap", action="store_true", help="Run undefined-behavior instrumentation with traps (no runtime library)")
    parser.add_argument("--worker", nargs=2, metavar=("LIB", "CASE"), help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.worker:
        lib = ctypes.CDLL(args.worker[0])
        case = getattr(lib, args.worker[1])
        case.restype = ctypes.c_int
        result = case()
        if result:
            print(f"FAIL {args.worker[1]}: C assertion line {result}", flush=True)
        return 1 if result else 0
    cc = shutil.which(args.cc or "clang")
    if not cc:
        print("TOOLCHAIN ERROR: provide a C99 compiler with --cc", file=sys.stderr)
        return 2
    cc = Path(cc).resolve()
    windows = os.name == "nt"
    exe_mode = windows and args.arch == "32"
    linker = cc.with_name("lld.exe")
    if windows and not linker.is_file():
        print("TOOLCHAIN ERROR: Windows freestanding runner requires adjacent lld.exe", file=sys.stderr)
        return 2
    invoke([cc, "--version"])
    common = ["-std=c99", "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
              "-Wall", "-Wextra", "-Werror", "-Wconversion", "-Wsign-conversion",
              "-I" + str(DEVICE), "-I" + str(ROOT / "tests"),
              "-I" + str(ROOT / "User/BSP"), "-I" + str(ROOT / "User/BSP/Ports"),
              f"-DCONFIG_ILI9488_TX_UNITS={args.units}"]
    if not windows:
        common += ["-fPIC"]
    if args.arch != "native":
        common += ["-m" + args.arch]
    if args.ubsan_trap:
        common += ["-fsanitize=undefined", "-fsanitize-trap=all"]
    suites = ["api", "transport", "pixels", "lifecycle", "boardport", "candidate", "graphics"] if args.suite == "all" else [args.suite]
    test_sources = [ROOT / "tests" / f"test_{suite}.c" for suite in suites]
    # Missing requested suites are failures; never silently reduce the test scope.
    for path in test_sources:
        if not path.is_file():
            print(f"MISSING TEST SUITE: {path}", file=sys.stderr)
            return 1
    tests = [(path.name, name) for path in test_sources
             for name in re.findall(r"^TEST\((\w+)\)", path.read_text(encoding="utf-8"), re.M)]
    if not tests:
        raise RuntimeError("No test cases discovered")
    sources = [DEVICE / "drv_ili9488.c", DEVICE / "drv_ili9488_panel.c",
               ROOT / "tests/mock_port.c", ROOT / "tests/runtime.c",
               ROOT / "tests/fixtures/panel_test.c", *test_sources]
    if "boardport" in suites:
        sources.append(ROOT / "User/BSP/Ports/bsp_ili9488_fsmc_port.c")
    if "candidate" in suites:
        sources.append(DEVICE / "drv_ili9488_panel_demo.c")
    if "graphics" in suites:
        sources.extend([ROOT / "User/Module/Graphics/gfx_text.c", ROOT / "User/Module/Graphics/gfx_font8x16.c", ROOT / "User/Services/Display/gfx_ili9488.c"])
        common.extend(["-I" + str(ROOT / "User/Module/Graphics"), "-I" + str(ROOT / "User/Services/Display")])
    demo = ROOT / "examples/host_demo.c"
    if args.suite == "all":
        example_test = ROOT / "tests/test_example.c"
        if not demo.is_file() or not example_test.is_file():
            print("MISSING REQUIRED EXAMPLE: examples/host_demo.c and tests/test_example.c are mandatory for --suite all", file=sys.stderr)
            return 1
        sources.extend([demo, example_test])
        tests.append(("test_example.c", "test_host_example"))
    total = 0
    for opt in (["0", "2"] if args.opt == "all" else [args.opt]):
        out = ROOT / "build" / f"host-{args.arch}-O{opt}-u{args.units}{'-ubsan' if args.ubsan_trap else ''}"
        out.mkdir(parents=True, exist_ok=True)
        # First verify the compiler/linker/loader independently of product code.
        probe = out / "probe.c"
        probe.write_text("int entry(void) { return sizeof(void*) == 4 ? 0 : 1; }\n" if exe_mode else
                         "__declspec(dllexport) int probe(void) { return 17; }\n" if windows
                         else "int probe(void) { return 17; }\n", encoding="utf-8")
        probe_obj = out / "probe.o"
        probe_lib = out / ("probe.exe" if exe_mode else "probe.dll" if windows else "probe.so")
        try:
            invoke([cc, *common, "-c", probe, "-o", probe_obj])
            if exe_mode:
                invoke([linker, "-flavor", "link", "/entry:entry", "/subsystem:console", "/nodefaultlib", "/machine:x86", f"/out:{probe_lib}", probe_obj])
                invoke([probe_lib])
            elif windows:
                invoke([linker, "-flavor", "link", "/dll", "/noentry", "/nodefaultlib", f"/out:{probe_lib}", probe_obj])
            else:
                invoke([cc, "-shared", probe_obj, "-o", probe_lib])
            if not exe_mode and ctypes.CDLL(str(probe_lib)).probe() != 17:
                raise RuntimeError("probe returned wrong value")
        except (OSError, subprocess.SubprocessError, RuntimeError) as exc:
            print(f"TOOLCHAIN ERROR: {exc}", file=sys.stderr)
            return 2
        objects = []
        # Public headers must be independently usable without HAL/RTOS.
        for header in ["drv_ili9488_types.h", "drv_ili9488_port.h", "drv_ili9488.h", "drv_ili9488_panels.h"]:
            header_probe = out / "header.c"
            header_probe.write_text(f'#include "{header}"\n', encoding="utf-8")
            invoke([cc, *common, "-fsyntax-only", header_probe])
        for i, source in enumerate(sources):
            obj = out / f"{i}.o"
            invoke([cc, *common, "-O" + opt, "-c", source, "-o", obj])
            objects.append(obj)
        library = out / ("tests.dll" if windows else "tests.so")
        if exe_mode:
            for filename, name in tests:
                entry = out / "entry.o"
                executable = out / (name + ".exe")
                invoke([cc, *common, "-O" + opt, "-DILI9488_TEST_CASE=" + name,
                        "-c", ROOT / "tests/test_main.c", "-o", entry])
                invoke([linker, "-flavor", "link", "/entry:entry", "/subsystem:console", "/nodefaultlib",
                        "/machine:x86", f"/out:{executable}", entry, *objects])
                result = subprocess.run([str(executable)], timeout=60, check=False)
                if result.returncode:
                    raise RuntimeError(f"{filename}:{name}: assertion line/exception {result.returncode}")
                print(f"PASS x86 O{opt} units={args.units} {filename}:{name}", flush=True)
                total += 1
            continue
        if windows:
            invoke([linker, "-flavor", "link", "/dll", "/noentry", "/nodefaultlib", f"/out:{library}", *objects])
        else:
            invoke([cc, "-shared", *objects, "-o", library])
        for filename, name in tests:
            invoke([sys.executable, __file__, "--worker", library, name])
            print(f"PASS {args.arch} O{opt} units={args.units} {filename}:{name}", flush=True)
            total += 1
    print(f"PASS {total} case executions; each in a fresh process", flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.SubprocessError, RuntimeError) as error:
        print(f"TEST/BUILD ERROR: {error}", file=sys.stderr)
        sys.exit(1)
