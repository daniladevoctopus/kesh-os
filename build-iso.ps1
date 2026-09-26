$ErrorActionPreference = "Stop"
$Python = "D:\keshos-toolchain\KeshBE\bin\python.exe"
if (-not (Test-Path $Python)) { $Python = "python" }
& $Python tools\make_iso.py
if ($LASTEXITCODE -ne 0) {
    Write-Error "ISO packaging failed with code $LASTEXITCODE"
    exit $LASTEXITCODE
}
Write-Host "KeshOS ISO created successfully: build\keshos.iso"
