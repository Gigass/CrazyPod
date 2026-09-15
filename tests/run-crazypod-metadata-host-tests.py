#!/usr/bin/env python3
"""Parse a synthetic MP4 with the firmware's own parser, on the host.

The metadata parsers are plain C over open/read/lseek, so they build
against rbcodec's Unix platform shim without a device. -Wno-unused-result
covers inherited Rockbox code that ignores read()'s return value.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
flags = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
         "-Wno-unused-result", "-D_GNU_SOURCE",
         "-I" + str(root / "tests/crazypod-metadata-stubs"),
         "-I" + str(root / "lib/rbcodec"),
         "-I" + str(root / "firmware/include"),
         "-I" + str(root / "lib/rbcodec/metadata")]
sources = [root / "lib/rbcodec/metadata/mp4.c",
           root / "lib/rbcodec/metadata/metadata_common.c",
           root / "tests/crazypod_mp4_metadata_host_test.c"]
with tempfile.TemporaryDirectory(prefix="crazypod-metadata-") as temporary:
    executable = Path(temporary) / ("mp4" + (".exe" if os.name == "nt" else ""))
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + flags +
                   list(map(str, sources)) + ["-o", str(executable)],
                   check=True)
    subprocess.run([str(executable)], check=True)
