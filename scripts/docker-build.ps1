# Build rpc compiler via Docker for Linux.
$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot)

$Image = "rpc-linux"
$Dist  = "dist"

docker build --target linux-builder -t $Image .
New-Item -ItemType Directory -Force -Path $Dist | Out-Null

$Tmp = docker create $Image
try {
    docker cp "${Tmp}:/src/build/compiler/rpc" "$Dist/rpc"
} finally {
    docker rm $Tmp 2>$null
}

Write-Host "Artifact:"
Get-ChildItem "$Dist/rpc" | Format-Table Name, Length
