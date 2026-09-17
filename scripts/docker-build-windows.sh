#!/usr/bin/env bash
# Build rpc compiler via Docker for Windows (MinGW cross-compile).
set -euo pipefail

cd "$(dirname "$0")/.."
IMAGE="rpc-windows"
DIST="dist"

# See docker-build.sh: the context has no .git/, so the version must be injected.
VERSION="$(scripts/version.sh)"
COMMIT="$(git rev-parse --short=7 HEAD)"

docker build --target windows-builder -t "$IMAGE" \
    --build-arg RPC_VERSION_STRING="$VERSION" \
    --build-arg RPC_BUILD_COMMIT="$COMMIT" .
mkdir -p "$DIST"

TMP=$(docker create "$IMAGE")
trap 'docker rm "$TMP" 2>/dev/null' EXIT
docker cp "$TMP":/src/build/compiler/rpc.exe "$DIST/rpc.exe"

echo "Artifact:"
ls -lh "$DIST/rpc.exe"
