@echo off
setlocal

set "PYTHON=python"
set "NINJA=ninja"
if exist "D:\keshos-toolchain\KeshBE\bin\python.exe" set "PYTHON=D:\keshos-toolchain\KeshBE\bin\python.exe"
if exist "D:\keshos-toolchain\KeshBE\bin\ninja.exe" set "NINJA=D:\keshos-toolchain\KeshBE\bin\ninja.exe"

%PYTHON% generate_ninja.py
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
%NINJA% iso
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo [+] Successfully built build\keshos.iso!
endlocal
