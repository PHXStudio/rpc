#!/usr/bin/env bash
# Build rpc compiler via Docker for Windows (MinGW cross-compile).
set -euo pipefail

cd "$(dirname "$0")/.."
IMAGE="rpc-windows"
DIST="dist"

docker build --target windows-builder -t "$IMAGE" .
mkdir -p "$DIST"

TMP=$(docker create "$IMAGE")
trap 'docker rm "$TMP" 2>/dev/null' EXIT
docker cp "$TMP":/src/build/compiler/rpc.exe "$DIST/rpc.exe"

echo "Artifact:"
ls -lh "$DIST/rpc.exe"
