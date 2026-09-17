#!/usr/bin/env bash
# Print the release version for HEAD.
#
# Single source of truth for the version string, shared by the docker-build
# scripts and by CI -- three copies of this logic would drift, and a drifted
# version is exactly the thing this whole mechanism exists to prevent.
#
# HEAD on a v* tag  -> the tag name
# otherwise         -> 1.0.<commit count>-<short sha>
set -euo pipefail

cd "$(dirname "$0")/.."

# A shallow clone makes rev-list --count return 1, which yields "1.0.1-<sha>":
# wrong, plausible-looking, and silent. Refuse instead of emitting it.
if [ "$(git rev-parse --is-shallow-repository)" = "true" ]; then
    echo "version.sh: shallow clone; commit count would be wrong (need full history)" >&2
    exit 1
fi

# --match is load-bearing. The rolling "latest" release tag always points at the
# tip of main, so an unqualified `describe --exact-match` returns the literal
# string "latest" for exactly the commits we care about most.
TAG="$(git describe --tags --exact-match --match 'v[0-9]*' HEAD 2>/dev/null || true)"
if [ -n "$TAG" ]; then
    echo "$TAG"
    exit 0
fi

# A binary built from a modified tree must not claim a clean commit. Without
# this the Docker path would report a pristine version for a dirty tree while a
# local CMake build of the same tree appended -dirty, so "1.0.43-abc1234" would
# name two different artifacts depending on how it was built.
DIRTY=""
if [ -n "$(git status --porcelain)" ]; then
    DIRTY="-dirty"
fi

echo "1.0.$(git rev-list --count HEAD)-$(git rev-parse --short=7 HEAD)${DIRTY}"
