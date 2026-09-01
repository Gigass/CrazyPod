#!/usr/bin/env python3
"""Run GB/GBC core and storage tests without a device or copyrighted ROMs."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
core = [root / f"apps/plugins/rockboy/{name}.c" for name in
        ("cpu", "fastmem", "hw", "lcd", "lcdc", "mem", "rtc", "sound")]
core += [root / f"apps/crazypod/gameboy/crazypod_gameboy_{name}.c"
         for name in ("core", "cartridge")]
flags = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
         "-fno-strict-aliasing", "-DCRAZYPOD_GAMEBOY_CORE",
         "-I" + str(root / "tests/crazypod-gameboy-stubs"),
         "-I" + str(root / "apps/crazypod")]
with tempfile.TemporaryDirectory(prefix="crazypod-gameboy-") as temporary:
    for name in ("core", "storage", "screen"):
        sources = list(core)
        test_flags = list(flags)
        if name == "storage":
            sources += [root / "apps/crazypod/gameboy/crazypod_gameboy.c",
                        root / "apps/crazypod/miniapps/installer/"
                               "crazypod_sha256.c"]
        if name == "screen":
            sources = [root / "apps/crazypod/ui/features/miniapps/"
                              "crazypod_gameboy_screen.c"]
            test_flags = ["-I" + str(root / "tests/crazypod-gameboy-ui-stubs"),
                          "-I" + str(root / "lib/lvgl")] + flags
        sources += [root / f"tests/crazypod_gameboy_{name}_host_test.c"]
        executable = Path(temporary) / (name + (".exe" if os.name == "nt" else ""))
        subprocess.run(shlex.split(os.environ.get("CC", "cc")) + test_flags +
                       list(map(str, sources)) + ["-o", str(executable)],
                       check=True)
        subprocess.run([str(executable)], check=True)
