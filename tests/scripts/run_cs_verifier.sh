#!/usr/bin/env bash
# Run the C# cross-language verifier. Usage:
#   run_cs_verifier.sh <generated_cs_dir> [args passed to CrossLangVerifier...]
set -euo pipefail
GEN_CS="${1:?generated cs dir}"
shift
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ="${SCRIPT_DIR}/../cs/CrossLangVerifier.csproj"
exec dotnet run --project "$PROJ" -p:GeneratedCsDir="$GEN_CS" -- "$@"
