$ErrorActionPreference = "Stop"
$Python = if (Test-Path "D:\keshos-toolchain\KeshBE\bin\python.exe") { "D:\keshos-toolchain\KeshBE\bin\python.exe" } else { "python" }
$Ninja = if (Test-Path "D:\keshos-toolchain\KeshBE\bin\ninja.exe") { "D:\keshos-toolchain\KeshBE\bin\ninja.exe" } else { "ninja" }
& $Python generate_ninja.py
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Ninja iso
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "KeshOS ISO created successfully: build\keshos.iso"
