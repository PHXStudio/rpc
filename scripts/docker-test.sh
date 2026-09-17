#!/usr/bin/env bash
# Run rpc tests (gtest + cross-lang) via Docker on Linux.
set -euo pipefail

cd "$(dirname "$0")/.."
IMAGE="rpc-linux-test"

docker build --target linux-tester -t "$IMAGE" .
docker run --rm "$IMAGE"
