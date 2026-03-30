# Run rpc tests via Docker for Windows (MinGW cross-compile, gtest only).
$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot)

$Image = "rpc-windows-test"

docker build --target windows-builder -t $Image `
    --build-arg BUILD_TESTING=ON .

docker run --rm $Image cmake --build /src/build --target test
