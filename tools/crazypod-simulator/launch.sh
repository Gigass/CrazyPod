#!/bin/sh
set -eu

build_dir=$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)
cd "$build_dir"
exec "$build_dir/rockboxui" --nobackground "$@" >>/tmp/crazypod-ui.log 2>&1
