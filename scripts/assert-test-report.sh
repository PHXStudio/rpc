#!/usr/bin/env bash
# Fail unless a ctest report shows a complete, fully-exercised run.
#
# "100% tests passed" on its own is not evidence that the suite ran. Two real
# cases, both from CLAUDE.md rule six:
#
#   - Missing python3 means find_package(Python3) fails and the two Python
#     conformance tests are never registered. ctest still prints 100% passed,
#     while the Python backend was never executed. Only the total catches this.
#   - A missing Go toolchain leaves those tests registered but reporting
#     SKIP:, which ctest counts as a pass. Only the skip check catches this.
#
# Usage: assert-test-report.sh <ctest-output-file> [expected-test-count]
set -euo pipefail

REPORT="${1:?usage: assert-test-report.sh <ctest-output-file> [expected-count]}"
EXPECTED="${2:-}"

if [ ! -f "$REPORT" ]; then
    echo "assert-test-report: no such report: $REPORT" >&2
    exit 1
fi

fail=0
note() { echo "assert-test-report: $1" >&2; fail=1; }

# 1. The summary line must exist and report no failures.
SUMMARY="$(grep -E '^[0-9]+% tests passed' "$REPORT" || true)"
if [ -z "$SUMMARY" ]; then
    note "no ctest summary line found -- did the suite actually run?"
else
    echo "assert-test-report: $SUMMARY"
    case "$SUMMARY" in
        "100% tests passed"*) ;;
        *) note "not every test passed" ;;
    esac
    case "$SUMMARY" in
        *"0 tests failed out of"*) ;;
        *) note "ctest reports failed tests" ;;
    esac
fi

# 2. Total count. A wrong count means a target was never configured or built --
#    it does not mean the missing tests passed.
if [ -n "$EXPECTED" ]; then
    # Bash's own regex rather than sed: GNU sed accepts \+ in a BRE and BSD sed
    # does not, so a sed one-liner here reads correctly on Linux and silently
    # extracts nothing on macOS.
    TOTAL=""
    if [[ "$SUMMARY" =~ out\ of\ ([0-9]+) ]]; then
        TOTAL="${BASH_REMATCH[1]}"
    fi
    if [ -z "$TOTAL" ]; then
        note "could not read the test total from the summary line"
    elif [ "$TOTAL" != "$EXPECTED" ]; then
        note "expected $EXPECTED tests, ctest ran $TOTAL"
    else
        echo "assert-test-report: total $TOTAL, as expected"
    fi
fi

# 3. Skips. A skipped test means the toolchain for that backend is missing, so
#    that backend was not verified in this run.
SKIPPED="$(grep -c '\*\*\*Skipped' "$REPORT" || true)"
if [ "$SKIPPED" != "0" ]; then
    note "$SKIPPED skipped test(s); a skipped backend is an unverified backend"
    grep '\*\*\*Skipped' "$REPORT" >&2 || true
else
    echo "assert-test-report: no skipped tests"
fi

if [ "$fail" != "0" ]; then
    echo "assert-test-report: FAILED" >&2
    exit 1
fi

echo "assert-test-report: OK"
