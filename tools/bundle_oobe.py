import os
import base64

oobe_dir = r'c:\Users\DanDevXP\Desktop\keshoos\keshos_oobe'

with open(os.path.join(oobe_dir, 'index.html'), 'r', encoding='utf-8') as f:
    html = f.read()

with open(os.path.join(oobe_dir, 'style.css'), 'r', encoding='utf-8') as f:
    css = f.read()

with open(os.path.join(oobe_dir, 'app.js'), 'r', encoding='utf-8') as f:
    js = f.read()

with open(os.path.join(oobe_dir, 'logo.png'), 'rb') as f:
    logo_b64 = base64.b64encode(f.read()).decode('ascii')

# Inline all assets
bundle = html
bundle = bundle.replace('<link rel="stylesheet" href="style.css">', f'<style>\n{css}\n</style>')
bundle = bundle.replace('src="logo.png"', f'src="data:image/png;base64,{logo_b64}"')
bundle = bundle.replace('<script src="app.js"></script>', f'<script>\n{js}\n</script>')

bundle_path = os.path.join(oobe_dir, 'oobe_bundle.html')
with open(bundle_path, 'w', encoding='utf-8') as f:
    f.write(bundle)

print(f'Successfully built oobe_bundle.html ({len(bundle)} bytes)!')
