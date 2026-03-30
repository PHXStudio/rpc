#!/usr/bin/env bash
# Build rpc compiler via Docker for Linux.
set -euo pipefail

cd "$(dirname "$0")/.."
IMAGE="rpc-linux"
DIST="dist"

docker build --target linux-builder -t "$IMAGE" .
mkdir -p "$DIST"

TMP=$(docker create "$IMAGE")
trap 'docker rm "$TMP" 2>/dev/null' EXIT
docker cp "$TMP":/src/build/compiler/rpc "$DIST/rpc"

echo "Artifact:"
ls -lh "$DIST/rpc"
