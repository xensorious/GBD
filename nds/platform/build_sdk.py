#!/usr/bin/env python3
"""Build the pinned open-source DS libraries using standard ARM GCC."""
from pathlib import Path
import concurrent.futures
import os
import subprocess
import sys

root = Path(sys.argv[1]).resolve()
src = root / "src/libnds"
cc = os.environ["NDS_CC"]
ar = cc.replace("gcc", "ar")
inc = ["-I" + str(src / "include"), "-I" + str(src / "source/common"),
       "-I" + str(root / "sysroot/usr/include/newlib")]
# These optional debug/console components require the devkitPro syscall ABI or
# generated console-font data. The game has its own renderer and UI.
skip = {"console.c", "keyboard.c", "initSystem.c", "gurumeditation.c", "sassert.c"}

(src / "include/nds/libversion.h").write_text(
    '#ifndef __LIBNDSVERSION_H__\n#define __LIBNDSVERSION_H__\n'
    '#define _LIBNDS_MAJOR_ 1\n#define _LIBNDS_MINOR_ 8\n'
    '#define _LIBNDS_PATCH_ 3\n#define _LIBNDS_STRING "libnds 1.8.3"\n#endif\n')

for cpu, chip in [(9, "arm946e-s"), (7, "arm7tdmi")]:
    out = root / f"build/sdk-arm{cpu}"
    out.mkdir(parents=True, exist_ok=True)
    paths = list((src / "source/common").glob("*.[cs]"))
    paths += list((src / f"source/arm{cpu}").glob("*.[cs]"))
    if cpu == 9:
        paths += list((src / "source/arm9/system").glob("*.[cs]"))
        paths += list((src / "source/arm9/dldi").glob("*.[cs]"))
    flags = ["-Os", "-g", "-mthumb", "-mthumb-interwork", f"-mcpu={chip}",
             f"-DARM{cpu}", "-DNDEBUG", "-ffunction-sections", "-fdata-sections",
             "-fno-strict-aliasing", "-Wno-incompatible-pointer-types",
             "-Wno-int-conversion"] + inc

    def one(path):
        if path.name in skip:
            return None
        obj = out / (str(path.relative_to(src / "source")).replace("/", "_") + ".o")
        command = [cc] + flags
        if path.suffix == ".s":
            command += ["-x", "assembler-with-cpp"]
        subprocess.run(command + ["-c", str(path), "-o", str(obj)], check=True)
        return str(obj)

    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        objects = list(pool.map(one, paths))
    library = root / f"lib/libnds{cpu}.a"
    library.parent.mkdir(exist_ok=True)
    subprocess.run([ar, "rcs", str(library)] + [x for x in objects if x], check=True)

native = root / "build/ndstool"
native.mkdir(parents=True, exist_ok=True)
objects = []
for path in (root / "src/ndstool/source").glob("*.c"):
    obj = native / (path.name + ".o")
    subprocess.run(["gcc", "-O2", "-c", str(path), "-o", str(obj)], check=True)
    objects.append(str(obj))
(root / "bin").mkdir(exist_ok=True)
subprocess.run(["g++", "-O2", '-DPACKAGE_VERSION="2.3.1"', "-o",
                str(root / "bin/ndstool")] +
               list(map(str, (root / "src/ndstool/source").glob("*.cpp"))) + objects,
               check=True)
print("Built libnds ARM9/ARM7 and ndstool.")
