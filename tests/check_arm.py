"""Compile the production units for Cortex-M4; no firmware link or hardware access."""
import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", required=True, help="Clang with the ARM target")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = root / "build/board-arm"
    out.mkdir(parents=True, exist_ok=True)
    sources = ["User/Device/ILI9488/drv_ili9488.c", "User/Device/ILI9488/drv_ili9488_panel.c",
               "User/Device/ILI9488/drv_ili9488_panel_demo.c",
               "User/BSP/Ports/bsp_ili9488_fsmc_port.c", "User/BSP/bsp_lcd8080_puzhong_f407.c"]
    sources[0:0] = ["User/Module/Graphics/gfx_text.c", "User/Module/Graphics/gfx_font8x16.c", "User/Services/Display/gfx_ili9488.c"]
    flags = ["-target", "arm-none-eabi", "-mcpu=cortex-m4", "-mthumb", "-std=c99", "-ffreestanding",
             "-Wall", "-Wextra", "-Werror", "-Wconversion", "-Wsign-conversion", "-O2"]
    for directory in ["User/Device/ILI9488", "User/BSP", "User/BSP/Ports", "User/Module/Graphics", "User/Services/Display"]:
        flags.append("-I" + str(root / directory))
    for relative in sources:
        source = root / relative
        subprocess.run([args.cc, *flags, "-c", str(source), "-o", str(out / (source.stem + ".o"))],
                       check=True, timeout=60)
        print("PASS Cortex-M4 object", source.name)
    subprocess.run([args.cc, *flags, "-S", str(root / sources[-1]), "-o", str(out / "bus.s")],
                   check=True, timeout=60)
    print("Assembly for inspection:", out / "bus.s")


if __name__ == "__main__":
    main()
