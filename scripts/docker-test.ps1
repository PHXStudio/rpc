# Run rpc tests (gtest + cross-lang) via Docker on Linux.
$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot)

$Image = "rpc-linux-test"

docker build --target linux-tester -t $Image .
docker run --rm $Image
