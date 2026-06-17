#!/usr/bin/env bash
# scripts/package.sh [OUT_DIR] — produce a version-stamped source tarball.
# Version comes from the latest git tag, so the filename reflects exactly what
# you're shipping (e.g. grunt-v0.2.3.tar.gz).
#
# OUT_DIR defaults to the repo's parent (local convenience). CI passes an
# explicit dir so the artifact lands somewhere predictable.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
REPO_NAME="$(basename "$REPO_DIR")"
PARENT_DIR="$(dirname "$REPO_DIR")"

OUT_DIR="${1:-$PARENT_DIR}"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

VER="$(git -C "$REPO_DIR" describe --tags --abbrev=0 2>/dev/null || echo v0.0.0-untagged)"
OUT="$OUT_DIR/${REPO_NAME}-${VER}.tar.gz"

cd "$PARENT_DIR"
tar czf "$OUT" \
    --exclude="$REPO_NAME/build" \
    --exclude="$REPO_NAME/build_sample" \
    --exclude="$REPO_NAME/build_check" \
    --exclude="$REPO_NAME/dist" \
    --exclude="$REPO_NAME/third_party/imgui" \
    --exclude="$REPO_NAME/third_party/miniaudio" \
    --exclude="$REPO_NAME/third_party/stb" \
    "$REPO_NAME"

echo "$OUT"
