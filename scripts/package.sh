#!/usr/bin/env bash
# scripts/package.sh — produce a version-stamped source tarball.
# The version comes from the latest git tag, so the filename always reflects
# exactly what you're about to push (e.g. grunt-v0.2.1.tar.gz).
set -euo pipefail

cd "$(dirname "$0")/.."
VER="$(git describe --tags --abbrev=0 2>/dev/null || echo v0.0.0-untagged)"
OUT="grunt-${VER}.tar.gz"

# package the repo dir from its parent so paths are grunt/...
cd ..
tar czf "$OUT" \
    --exclude='grunt/build' \
    --exclude='grunt/build_sample' \
    --exclude='grunt/third_party/imgui' \
    --exclude='grunt/third_party/miniaudio' \
    grunt
echo "wrote $(pwd)/$OUT"
