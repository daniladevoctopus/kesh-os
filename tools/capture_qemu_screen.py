"""
Capture real frame screenshot from QEMU monitor.
"""
import subprocess, time, os

keshos_root = r'c:\Users\DanDevXP\Desktop\keshoos'
iso_path = os.path.join(keshos_root, 'releases', 'KeshOS-Beta-0.8.0-Brownie.iso')
qemu_exe = r'D:\qemu\qemu-system-x86_64.exe'
ppm_out = os.path.join(keshos_root, 'out', 'qemu_frame.ppm')
os.makedirs(os.path.dirname(ppm_out), exist_ok=True)

# Kill any running QEMU
subprocess.run(['taskkill', '/F', '/IM', 'qemu-system-x86_64.exe'], capture_output=True)

# Launch QEMU with monitor on pipe or stdin
proc = subprocess.Popen(
    [qemu_exe, '-m', '2048', '-cdrom', iso_path, '-boot', 'd', '-cpu', 'max', '-vga', 'std', '-display', 'none', '-monitor', 'stdio'],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
)

print("QEMU running... Waiting 2 seconds for boot screen...")
time.sleep(2)

# Send screendump command to QEMU monitor
proc.stdin.write(f"screendump {ppm_out}\n")
proc.stdin.flush()
time.sleep(1)

# Send Enter key or inspect
proc.stdin.write("sendkey ret\n")
proc.stdin.flush()
time.sleep(3)

ppm_out2 = os.path.join(keshos_root, 'out', 'qemu_frame_after_boot.ppm')
proc.stdin.write(f"screendump {ppm_out2}\n")
proc.stdin.flush()
time.sleep(1)

proc.stdin.write("quit\n")
proc.stdin.flush()
proc.wait()

print(f"Screenshots captured:")
if os.path.exists(ppm_out):
    print(f"  [+] Initial Frame: {ppm_out} ({os.path.getsize(ppm_out)} bytes)")
if os.path.exists(ppm_out2):
    print(f"  [+] Boot Frame: {ppm_out2} ({os.path.getsize(ppm_out2)} bytes)")
