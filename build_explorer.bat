@echo off
set "PATH=C:\RosBE\bin;C:\RosBE\i386\bin;%PATH%"
echo [BUILD] Building KeshOS Explorer and RShell with Ninja...
ninja -C output-MinGW-i386 explorer rshell
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] KeshOS explorer.exe built successfully!
) else (
    echo [ERROR] Build failed with code %ERRORLEVEL%
)
