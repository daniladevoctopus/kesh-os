# Let's inspect live OOBE DOM via Chrome DevTools WebSocket in the VM!
cat << 'EOF' > /tmp/inspect_live_oobe.py
import urllib.request
import json
import os

try:
    # 1. Check all DevTools targets
    resp = urllib.request.urlopen('http://127.0.0.1:9229/json')
    targets = json.loads(resp.read().decode())
    print("=== LIVE DEVTOOLS TARGETS ===")
    for t in targets:
        print(f"[{t.get('type')}] {t.get('title')} -> {t.get('url')}")
except Exception as e:
    print("DevTools error:", e)

# 2. List all PAK files
print("\n=== ALL PAK FILES ===")
for root, dirs, files in os.walk('/opt/google/chrome'):
    for f in files:
        if f.endswith('.pak'):
            p = os.path.join(root, f)
            print(f"  {p} ({os.path.getsize(p):,} bytes)")

EOF
python3 /tmp/inspect_live_oobe.py
