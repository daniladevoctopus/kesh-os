@echo off
echo [*] Building KeshOS 1.0 Drop...
D:\keshos-toolchain\KeshBE\bin\ninja.exe
if %ERRORLEVEL% NEQ 0 (
    echo [!] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo [*] Packing ISO...
D:\keshos-toolchain\KeshBE\bin\python.exe tools\make_iso.py
if %ERRORLEVEL% NEQ 0 (
    echo [!] ISO packaging failed!
    pause
    exit /b %ERRORLEVEL%
)

echo [+] Successfully built build\keshos.iso!
