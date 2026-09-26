@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo        Kesh Package Manager (KPM) Windows Installer
echo ========================================================
echo.

set "KPM_DIR=%~dp0"
:: Remove trailing backslash if present
if "%KPM_DIR:~-1%"=="\" set "KPM_DIR=%KPM_DIR:~0,-1%"

echo Installing KPM from: !KPM_DIR!
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$kpm = [System.IO.Path]::GetFullPath('%KPM_DIR%');" ^
    "$userPath = [Environment]::GetEnvironmentVariable('Path', 'User');" ^
    "if ($userPath -split ';' -notcontains $kpm) {" ^
    "    $newPath = $userPath + ';' + $kpm;" ^
    "    [Environment]::SetEnvironmentVariable('Path', $newPath, 'User');" ^
    "    Write-Host '[SUCCESS] Added ' $kpm ' to User PATH.' -ForegroundColor Green;" ^
    "} else {" ^
    "    Write-Host '[INFO] KPM is already present in User PATH.' -ForegroundColor Yellow;" ^
    "}"

echo.
echo ========================================================
echo Installation complete! 
echo Open a new Command Prompt or PowerShell and type:
echo.
echo     kpm install notepad.kea
echo.
echo Packages will be downloaded into your Downloads\KPM folder!
echo ========================================================
pause
