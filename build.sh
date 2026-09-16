#!/bin/sh
# build.sh
#
# Configure, build, run the unit tests, then launch the demo.
# Any arguments given to this script are forwarded to firmware_demo,
# e.g.:
#   ./build.sh --target-temp 55 --critical-temp 80

set -e

BUILD_DIR="build"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j

echo ""
echo "== running unit tests =="
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo ""
echo "== running demo =="
"$BUILD_DIR/firmware_demo" "$@"
