#!/usr/bin/env bash
# Build rpc compiler via Docker for Linux.
set -euo pipefail

cd "$(dirname "$0")/.."
IMAGE="rpc-linux"
DIST="dist"

# The build context excludes .git/ (.dockerignore), so the compiler cannot work
# out which commit it came from -- it has to be told.
VERSION="$(scripts/version.sh)"
COMMIT="$(git rev-parse --short=7 HEAD)"

docker build --target linux-builder -t "$IMAGE" \
    --build-arg RPC_VERSION_STRING="$VERSION" \
    --build-arg RPC_BUILD_COMMIT="$COMMIT" .
mkdir -p "$DIST"

TMP=$(docker create "$IMAGE")
trap 'docker rm "$TMP" 2>/dev/null' EXIT
docker cp "$TMP":/src/build/compiler/rpc "$DIST/rpc"

echo "Artifact:"
ls -lh "$DIST/rpc"
