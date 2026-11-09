#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
HANDOFF=${1:?usage: run_focused.sh /absolute/path/RAGEBOT_HANDOFF_V10}
BUILD="$ROOT/continuation/evidence/wave8/angle/.build"
cd "$ROOT"
mkdir -p "$BUILD"
set -x
pwd
git rev-parse HEAD
g++ --version | head -n 1
objdump --version | head -n 1
sha256sum continuation/include/rage/angle_slice.hpp continuation/src/angle_slice.cpp continuation/tests/test_angle_slice.cpp continuation/evidence/wave8/angle/source_anchors.json continuation/evidence/wave8/angle/verify_source_bytes.py
FLAGS='-std=c++17 -g -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off'
g++ $FLAGS -O2 -Icontinuation/include continuation/src/angle_slice.cpp continuation/tests/test_angle_slice.cpp -o "$BUILD/test_angle_slice"
"$BUILD/test_angle_slice"
g++ $FLAGS -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -Icontinuation/include continuation/src/angle_slice.cpp continuation/tests/test_angle_slice.cpp -o "$BUILD/test_angle_slice_sanitize"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$BUILD/test_angle_slice_sanitize"
python3 -B continuation/evidence/wave8/angle/verify_source_bytes.py --handoff "$HANDOFF"
