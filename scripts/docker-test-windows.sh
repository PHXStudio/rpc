#!/usr/bin/env bash
# Run rpc tests via Docker for Windows (MinGW cross-compile, gtest only).
set -euo pipefail

cd "$(dirname "$0")/.."
IMAGE="rpc-windows-test"

docker build --target windows-builder -t "$IMAGE" \
    --build-arg BUILD_TESTING=ON .

docker run --rm "$IMAGE" cmake --build /src/build --target test
