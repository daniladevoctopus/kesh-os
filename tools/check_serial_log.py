#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parent.parent
log_path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / 'serial.log'
expected = 'KESHOS-2026-09-29-BLACKSCREEN-V4'

if not log_path.exists():
    raise SystemExit(f'SERIAL LOG ERROR: {log_path} does not exist')
text = log_path.read_text(errors='replace')
lines = text.splitlines()

print(f'log: {log_path}')
print(f'lines: {len(lines)}')
print(f'build-id-present: {expected in text}')
print(f'klog-records: {len(re.findall(r"^\[(?:FATAL|ERROR|WARN|NOTICE|INFO|DEBUG|TRACE) L[1-7]\] #\d+", text, re.M))}')
print(f'legacy-format-records: {len(re.findall(r"^\[[0-9]+\.[0-9]+\] \[INFO \]", text, re.M))}')

if expected not in text:
    print('RESULT: STALE KERNEL / WRONG ISO. Expected runtime build fingerprint was not found.')
    raise SystemExit(2)

if '<KLOG> online level=7' not in text:
    print('RESULT: KERNEL BUILD ID MATCHED BUT LOGGER BOOT MARKER WAS NOT FOUND.')
    raise SystemExit(3)

if re.search(r'^\[(?:FATAL|ERROR) L[12]\]|\[HALT\]|\[PANIC\]|security self-test=FAIL', text, re.M):
    print('RESULT: BOOT FAILED. Kernel reported an error, panic or halt.')
    raise SystemExit(4)

required = (
    'kernel_main entered; SSE path enabled',
    'security self-test=PASS',
    'ring3 payload returned to kernel mode',
    '[DESKTOP] Native Desktop Active.',
    '[DESKTOP] Rendering desktop frame...',
)
missing = [marker for marker in required if marker not in text]
if missing:
    print('RESULT: BOOT INCOMPLETE. Missing: ' + ', '.join(missing))
    raise SystemExit(5)

print('RESULT: PASS. Kernel initialized and desktop rendering started.')
