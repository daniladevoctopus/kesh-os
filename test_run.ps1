$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = 'D:\qemu\qemu-system-x86_64.exe'
$psi.Arguments = '-cdrom build/keshos.iso -m 2048 -vga std -serial stdio -display none -no-shutdown -no-reboot'
$psi.RedirectStandardOutput = $true
$psi.UseShellExecute = $false
$p = [System.Diagnostics.Process]::Start($psi)
Start-Sleep -Seconds 3
$p.Kill()
$out = $p.StandardOutput.ReadToEnd()
Set-Content -Path "qemu_test.log" -Value $out
Write-Output $out
