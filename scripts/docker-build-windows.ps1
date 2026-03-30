# Build rpc compiler via Docker for Windows (MinGW cross-compile).
$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot)

$Image = "rpc-windows"
$Dist  = "dist"

docker build --target windows-builder -t $Image .
New-Item -ItemType Directory -Force -Path $Dist | Out-Null

$Tmp = docker create $Image
try {
    docker cp "${Tmp}:/src/build/compiler/rpc.exe" "$Dist/rpc.exe"
} finally {
    docker rm $Tmp 2>$null
}

Write-Host "Artifact:"
Get-ChildItem "$Dist/rpc.exe" | Format-Table Name, Length
