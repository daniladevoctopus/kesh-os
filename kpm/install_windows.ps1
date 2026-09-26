$kpmDir = "C:\Users\DanDevXP\Desktop\keshoos\kpm"
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($userPath -split ';' -notcontains $kpmDir) {
    $newPath = $userPath.TrimEnd(';') + ';' + $kpmDir
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Host "[SUCCESS] KPM added to User PATH: $kpmDir" -ForegroundColor Green
} else {
    Write-Host "[INFO] KPM is already present in User PATH." -ForegroundColor Yellow
}
