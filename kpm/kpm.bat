<# :
@echo off
setlocal
set "KPM_BAT_DIR=%~dp0"
set "KPM_BAT_FILE=%~f0"
powershell -NoProfile -ExecutionPolicy Bypass -Command "& ([ScriptBlock]::Create((Get-Content -LiteralPath '%~f0' -Raw)))" %*
exit /b %errorlevel%
#>

# ==============================================================================
#  Kesh Package Manager (KPM) - Windows System Binary & Installer
#  Zero dependencies: Works out of the box on Windows 7 / 8 / 10 / 11
#  Packages are saved directly into %USERPROFILE%\Downloads\KPM
# ==============================================================================

$DefaultRepo = "https://keshos-kpm.vercel.app"
$GitHubMirror = "https://raw.githubusercontent.com/daniladevoctopus/kesh-os/main/kpm/server"
$ConfigFile = Join-Path $HOME ".kpm_config.json"
$DownDir = Join-Path $HOME "Downloads\KPM"
$InstallBinDir = Join-Path $env:LOCALAPPDATA "KeshOS\KPM\bin"
$InstalledKpmCmd = Join-Path $InstallBinDir "kpm.cmd"
$ScriptDir = if ($env:KPM_BAT_DIR) { $env:KPM_BAT_DIR.TrimEnd('\') } else { (Get-Location).Path }
$ThisFile = if ($env:KPM_BAT_FILE) { $env:KPM_BAT_FILE } else { $MyInvocation.MyCommand.Path }

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Get-Repo {
    if (Test-Path $ConfigFile) {
        try {
            $c = Get-Content $ConfigFile -Raw | ConvertFrom-Json
            if ($c.repo_url) { return $c.repo_url }
        } catch {}
    }
    return $DefaultRepo
}

function Print-Banner {
    Write-Host ""
    Write-Host "  _  ______  __  __" -ForegroundColor Cyan
    Write-Host " | |/ /  _ \|  \/  |   Kesh Package Manager (KPM)" -ForegroundColor Cyan
    Write-Host " | ' /| |_) | |\/| |   Windows CLI Edition" -ForegroundColor DarkCyan
    Write-Host " | . \|  __/| |  | |   Target:  $DownDir" -ForegroundColor DarkGray
    Write-Host " |_|\_\_|   |_|  |_|   Repo:    $(Get-Repo)" -ForegroundColor DarkGray
    Write-Host ""
}

function Notify-EnvironmentChange {
    try {
        if (-not ("Win32EnvChange" -as [type])) {
            Add-Type -TypeDefinition @"
            using System;
            using System.Runtime.InteropServices;
            public class WinEnvChange {
                [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
                public static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint Msg, UIntPtr wParam, string lParam, uint fuFlags, uint uTimeout, out UIntPtr lpdwResult);
            }
"@
        }
        $HWND_BROADCAST = [IntPtr]0xffff
        $WM_SETTINGCHANGE = 0x001a
        $result = [UIntPtr]::Zero
        [WinEnvChange]::SendMessageTimeout($HWND_BROADCAST, $WM_SETTINGCHANGE, [UIntPtr]::Zero, "Environment", 2, 2000, [ref]$result) | Out-Null
    } catch {}
}

function Install-ToSystem($quiet = $false) {
    if (!$quiet) {
        Write-Host "`n========================================================" -ForegroundColor Cyan
        Write-Host "  Kesh Package Manager (KPM) - One-Click System Setup" -ForegroundColor White
        Write-Host "========================================================" -ForegroundColor Cyan
    }

    # 1. Create target bin directory
    if (!(Test-Path $InstallBinDir)) {
        New-Item -ItemType Directory -Force -Path $InstallBinDir | Out-Null
    }

    # 2. Copy kpm.cmd and kpm.bat into the permanent PATH directory
    if (Test-Path $ThisFile) {
        Copy-Item -Path $ThisFile -Destination $InstalledKpmCmd -Force
        $kpmBat = Join-Path $InstallBinDir "kpm.bat"
        Copy-Item -Path $ThisFile -Destination $kpmBat -Force
    }

    # 3. Create Downloads\KPM folder
    if (!(Test-Path $DownDir)) {
        New-Item -ItemType Directory -Force -Path $DownDir | Out-Null
    }

    # 4. Add to User PATH
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $paths = if ($userPath) { $userPath -split ';' | Where-Object { $_ -ne "" } } else { @() }
    $added = $false
    if ($paths -notcontains $InstallBinDir) {
        $newUserPath = ($paths + $InstallBinDir) -join ';'
        [Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")
        $env:Path = "$InstallBinDir;$env:Path"
        Notify-EnvironmentChange
        $added = $true
    }

    if (!$quiet) {
        Write-Host "[+] Installed binary to: $InstalledKpmCmd" -ForegroundColor Green
        if ($added) {
            Write-Host "[+] Permanently added '$InstallBinDir' to User PATH!" -ForegroundColor Green
        } else {
            Write-Host "[*] '$InstallBinDir' is already configured in PATH." -ForegroundColor DarkCyan
        }
        Write-Host "[+] Package download folder ready: $DownDir" -ForegroundColor Green

        # 5. Pre-cache basic apps
        Write-Host "`n[*] Pre-caching core packages..." -ForegroundColor Yellow
        $null = Download-Package "notepad.kea" $true
        $null = Download-Package "explorer.kea" $true

        Write-Host "`n========================================================" -ForegroundColor Green
        Write-Host "  [SUCCESS] KPM IS NOW READY ON YOUR COMPUTER!" -ForegroundColor White
        Write-Host "========================================================" -ForegroundColor Green
        Write-Host "You can now DELETE this setup file if you want." -ForegroundColor Yellow
        Write-Host "From now on, just open ANY CMD or PowerShell window and type:" -ForegroundColor White
        Write-Host ""
        Write-Host "    kpm install notepad.kea" -ForegroundColor Cyan
        Write-Host "    kpm list" -ForegroundColor Cyan
        Write-Host "    kpm open" -ForegroundColor Cyan
        Write-Host "========================================================`n" -ForegroundColor Green

        Write-Host "Press [T] to test 'kpm list' in a new CMD window, or press Enter to finish..." -ForegroundColor DarkGray
        if ([Environment]::UserInteractive -and -not [Console]::IsInputRedirected) {
            try {
                $choice = Read-Host
                if ($choice -match '^[tT]') {
                    Start-Process "cmd.exe" -ArgumentList "/k kpm list"
                }
            } catch {}
        }
    }
}

function Parse-KeaHeader($path) {
    if (!(Test-Path $path)) { return $null }
    $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -lt 244) { return $null }
    $magic = [BitConverter]::ToUInt32($bytes, 0)
    if ($magic -ne 0x0141454B) { return $null }
    
    $enc = [Text.Encoding]::UTF8
    $name = $enc.GetString($bytes, 12, 32).Split([char]0)[0]
    $ver = $enc.GetString($bytes, 44, 16).Split([char]0)[0]
    $author = $enc.GetString($bytes, 60, 32).Split([char]0)[0]
    $cat = $enc.GetString($bytes, 92, 16).Split([char]0)[0]
    $desc = $enc.GetString($bytes, 108, 64).Split([char]0)[0]
    $entry = [BitConverter]::ToUInt64($bytes, 204)
    return @{
        Name = $name
        Version = $ver
        Author = $author
        Category = $cat
        Description = $desc
        Entry = $entry
        Size = $bytes.Length
    }
}

function Download-Package($pkg, $silent = $false) {
    if (!$pkg.EndsWith(".kea")) { $pkg = $pkg + ".kea" }
    
    if (!(Test-Path $DownDir)) {
        New-Item -ItemType Directory -Force -Path $DownDir | Out-Null
    }
    $dest = Join-Path $DownDir $pkg
    $repo = Get-Repo
    $primaryUrl = $repo.TrimEnd('/') + "/packages/" + $pkg
    $mirrorUrl = $GitHubMirror.TrimEnd('/') + "/packages/" + $pkg

    if (!$silent) {
        Write-Host "`n[*] Installing package: $pkg" -ForegroundColor Cyan
        Write-Host "[*] Target directory:  $dest" -ForegroundColor DarkGray
    }

    $downloaded = $false

    # 1. Try Primary Repo (Vercel)
    try {
        if (!$silent) { Write-Host "[*] Contacting repository ($primaryUrl)..." -ForegroundColor DarkGray }
        $req = [Net.HttpWebRequest]::Create($primaryUrl)
        $req.Timeout = 3500
        $req.ReadWriteTimeout = 3500
        $req.UserAgent = "KPM-Standalone/1.0"
        $resp = $req.GetResponse()
        $stream = $resp.GetResponseStream()
        $fileStream = [IO.File]::Create($dest)
        $stream.CopyTo($fileStream)
        $fileStream.Close()
        $stream.Close()
        $resp.Close()
        $downloaded = $true
        if (!$silent) { Write-Host "[+] Downloaded from Vercel repository!" -ForegroundColor Green }
    } catch {
        # 2. Try GitHub Mirror
        try {
            if (!$silent) { Write-Host "[*] Trying GitHub mirror..." -ForegroundColor Yellow }
            $req = [Net.HttpWebRequest]::Create($mirrorUrl)
            $req.Timeout = 3500
            $req.ReadWriteTimeout = 3500
            $req.UserAgent = "KPM-Standalone/1.0"
            $resp = $req.GetResponse()
            $stream = $resp.GetResponseStream()
            $fileStream = [IO.File]::Create($dest)
            $stream.CopyTo($fileStream)
            $fileStream.Close()
            $stream.Close()
            $resp.Close()
            $downloaded = $true
            if (!$silent) { Write-Host "[+] Downloaded from GitHub mirror!" -ForegroundColor Green }
        } catch {
            # 3. Try Local Fallbacks
            $candidates = @(
                (Join-Path $ScriptDir "server\packages\$pkg"),
                (Join-Path $ScriptDir "packages\$pkg"),
                (Join-Path $ScriptDir "..\build\apps\$pkg"),
                (Join-Path $ScriptDir "build\apps\$pkg"),
                (Join-Path $DownDir $pkg)
            )
            foreach ($candidate in $candidates) {
                if (Test-Path $candidate) {
                    $candResolved = (Resolve-Path $candidate).Path
                    $destResolved = if (Test-Path $dest) { (Resolve-Path $dest).Path } else { "" }
                    if ($candResolved -ne $destResolved) {
                        if (!$silent) { Write-Host "[+] Found local package build: $candidate" -ForegroundColor DarkCyan }
                        Copy-Item $candidate $dest -Force
                        $downloaded = $true
                        break
                    }
                }
            }
        }
    }

    if (!$downloaded -and (Test-Path $dest)) {
        # File already exists in Downloads\KPM
        $downloaded = $true
    }

    if (!$downloaded) {
        if (!$silent) {
            Write-Host "`n[X] Error: Package '$pkg' could not be found or downloaded." -ForegroundColor Red
            Write-Host "    Make sure internet connection is active or repo is deployed." -ForegroundColor Yellow
        }
        return $false
    }

    $info = Parse-KeaHeader $dest
    if ($info -and !$silent) {
        Write-Host "`n========================================================" -ForegroundColor DarkGray
        Write-Host "  PACKAGE VERIFICATION: $($info.Name) v$($info.Version)" -ForegroundColor Green
        Write-Host "========================================================" -ForegroundColor DarkGray
        Write-Host "  Author:      $($info.Author)" -ForegroundColor White
        Write-Host "  Category:    $($info.Category)" -ForegroundColor White
        Write-Host "  Summary:     $($info.Description)" -ForegroundColor White
        Write-Host "  Entry Point: 0x$($info.Entry.ToString('X'))" -ForegroundColor DarkGray
        Write-Host "  Format:      KEA Executable (64-bit KeshOS Binary)" -ForegroundColor Cyan
        Write-Host "  File Size:   $([math]::Round($info.Size/1KB, 1)) KB" -ForegroundColor DarkGray
        Write-Host "========================================================" -ForegroundColor DarkGray
        Write-Host "`n[SUCCESS] Package installed to: $dest`n" -ForegroundColor Green
    } elseif (!$silent) {
        Write-Host "`n[SUCCESS] Downloaded to: $dest`n" -ForegroundColor Green
    }
    return $true
}

function List-Packages {
    Write-Host "`n=== LOCALLY DOWNLOADED PACKAGES ($DownDir) ===" -ForegroundColor Cyan
    if (Test-Path $DownDir) {
        $files = Get-ChildItem -Path $DownDir -Filter *.kea
        if ($files.Count -gt 0) {
            foreach ($f in $files) {
                $hdr = Parse-KeaHeader $f.FullName
                $tag = if ($hdr) { "[$($hdr.Category)] $($hdr.Description)" } else { "KEA Binary" }
                Write-Host "  * $($f.Name.PadRight(20)) $([math]::Round($f.Length/1KB, 1).ToString().PadLeft(6)) KB  - $tag" -ForegroundColor Green
            }
        } else {
            Write-Host "  (Folder is empty. Run: kpm install notepad.kea)" -ForegroundColor DarkGray
        }
    } else {
        Write-Host "  (Folder is empty)" -ForegroundColor DarkGray
    }

    Write-Host "`n=== CLOUD REPOSITORY ($(Get-Repo)) ===" -ForegroundColor Cyan
    $fetched = $false
    try {
        $wc = New-Object Net.WebClient
        $wc.Headers.Add("User-Agent", "KPM-Standalone/1.0")
        $json = $wc.DownloadString((Get-Repo).TrimEnd('/') + "/packages.json") | ConvertFrom-Json
        foreach ($p in $json.packages) {
            $title = "$($p.name) v$($p.version)"
            Write-Host "  + $($title.PadRight(24)) [$($p.category)] - $($p.description)" -ForegroundColor White
        }
        $fetched = $true
    } catch {
        $localCatalog = Join-Path $ScriptDir "server\packages.json"
        if (Test-Path $localCatalog) {
            try {
                $json = Get-Content $localCatalog -Raw | ConvertFrom-Json
                foreach ($p in $json.packages) {
                    $title = "$($p.name) v$($p.version)"
                    Write-Host "  + $($title.PadRight(24)) [$($p.category)] - $($p.description)" -ForegroundColor White
                }
                $fetched = $true
            } catch {}
        }
    }

    if (!$fetched) {
        Write-Host "  + Notepad v1.0.0         [Utilities] - Native text editor for KeshOS" -ForegroundColor White
        Write-Host "  + Explorer v1.0.0        [System]    - File manager and browser for KeshOS" -ForegroundColor White
        Write-Host "  + Calculator v1.0.0      [Utilities] - Standard arithmetic calculator" -ForegroundColor White
        Write-Host "  + Doom Classic v1.1.0    [Games]     - Classic 1993 Doom engine bare-metal" -ForegroundColor White
    }
    Write-Host ""
}

function Show-Info($pkg) {
    if (!$pkg.EndsWith(".kea")) { $pkg = $pkg + ".kea" }
    $dest = Join-Path $DownDir $pkg
    $info = Parse-KeaHeader $dest
    if ($info) {
        Write-Host "`n=== .KEA Package: $($info.Name) ===" -ForegroundColor Cyan
        Write-Host "Version:     $($info.Version)"
        Write-Host "Author:      $($info.Author)"
        Write-Host "Category:    $($info.Category)"
        Write-Host "Description: $($info.Description)"
        Write-Host "Entry Point: 0x$($info.Entry.ToString('X'))"
        Write-Host "File Path:   $dest"
        Write-Host "Size:        $([math]::Round($info.Size/1KB, 1)) KB`n"
    } else {
        Write-Host "Package $pkg is not downloaded yet. Run: kpm install $pkg" -ForegroundColor Yellow
    }
}

function Open-Folder {
    if (!(Test-Path $DownDir)) {
        New-Item -ItemType Directory -Force -Path $DownDir | Out-Null
    }
    Invoke-Item $DownDir
    Write-Host "Opened: $DownDir" -ForegroundColor Green
}

function Uninstall-Kpm {
    Write-Host "`n[*] Uninstalling KPM from system..." -ForegroundColor Yellow
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $paths = $userPath -split ';' | Where-Object { $_ -ne "" -and $_ -ne $InstallBinDir }
    [Environment]::SetEnvironmentVariable("Path", ($paths -join ';'), "User")
    Notify-EnvironmentChange

    if (Test-Path $InstallBinDir) {
        Remove-Item -Path $InstallBinDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    Write-Host "[SUCCESS] KPM has been removed from PATH and AppData." -ForegroundColor Green
}

# ----------------- CLI Argument Handling -----------------
$argsList = @($args)

# If executed directly without parameters (e.g. double clicked in Explorer)
if ($argsList.Count -eq 0) {
    # Check if we are already running from the installed bin dir
    $isInstalled = ($ScriptDir.ToLower() -eq $InstallBinDir.ToLower())
    if (!$isInstalled) {
        # Perform 1-Click Install to PATH!
        Install-ToSystem -quiet $false
        exit 0
    } else {
        # Running from installed path without arguments -> print help
        Print-Banner
        Write-Host "Commands:"
        Write-Host "  kpm install <pkg>   Download package into Downloads\KPM" -ForegroundColor White
        Write-Host "  kpm list            List downloaded & available packages" -ForegroundColor White
        Write-Host "  kpm info <pkg>      Show package details" -ForegroundColor White
        Write-Host "  kpm open            Open Downloads\KPM folder" -ForegroundColor White
        Write-Host "  kpm uninstall       Remove KPM from PATH" -ForegroundColor White
        Write-Host ""
        exit 0
    }
}

$cmd = $argsList[0].ToLower()

switch ($cmd) {
    { $_ -in @("-h", "--help", "help") } {
        Print-Banner
        Write-Host "Usage:"
        Write-Host "  kpm install <package.kea>   Download package directly into Downloads\KPM" -ForegroundColor White
        Write-Host "  kpm list                    List downloaded and available packages" -ForegroundColor White
        Write-Host "  kpm info <package>          Inspect .KEA package metadata" -ForegroundColor White
        Write-Host "  kpm open                    Open Downloads\KPM folder in Explorer" -ForegroundColor White
        Write-Host "  kpm setup                   Install KPM permanently to Windows PATH" -ForegroundColor White
        Write-Host "  kpm uninstall               Remove KPM from Windows PATH" -ForegroundColor White
        Write-Host "  kpm repo <url>              Change package repository URL" -ForegroundColor White
        Write-Host ""
    }
    { $_ -in @("install", "i") } {
        if ($argsList.Count -lt 2) {
            Write-Host "[X] Error: Specify package name (e.g. kpm install notepad.kea)" -ForegroundColor Red
            exit 1
        }
        # Auto-ensure PATH is registered in background
        Install-ToSystem -quiet $true
        $null = Download-Package $argsList[1]
    }
    { $_ -in @("list", "ls") } {
        List-Packages
    }
    { $_ -in @("info", "show") } {
        if ($argsList.Count -lt 2) {
            Write-Host "Usage: kpm info <package.kea>" -ForegroundColor Red
            exit 1
        }
        Show-Info $argsList[1]
    }
    "open" {
        Open-Folder
    }
    { $_ -in @("setup", "install-kpm") } {
        Install-ToSystem -quiet $false
    }
    "uninstall" {
        Uninstall-Kpm
    }
    "repo" {
        if ($argsList.Count -ge 2) {
            @{ repo_url = $argsList[1] } | ConvertTo-Json | Set-Content $ConfigFile -Force
            Write-Host "Active repository updated to: $($argsList[1])" -ForegroundColor Green
        } else {
            Write-Host "Current repository: $(Get-Repo)" -ForegroundColor Cyan
        }
    }
    default {
        Write-Host "Unknown command: $cmd. Run 'kpm help' for usage." -ForegroundColor Red
        exit 1
    }
}
