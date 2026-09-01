#!/bin/sh
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$repo_root/tests/run-gameboy-host-tests.py"
