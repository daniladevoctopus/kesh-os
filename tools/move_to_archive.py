import os, shutil

root_dir = r'c:\Users\DanDevXP\Desktop\keshoos'
archive_dir = os.path.join(root_dir, 'archive')
os.makedirs(archive_dir, exist_ok=True)

keep_list = {
    'archive',
    'glaze_ui',
    'keshos_oobe',
    'tools',
    'MASTER_HANDOVER_KESHOS.md',
    'keshos_logo.png',
    'logo.png',
    'preview_glaze_ui.bat',
    'start_qemu.bat',
    'stop_qemu.bat',
    '.vscode',
}

for item in os.listdir(root_dir):
    if item in keep_list:
        continue
    src = os.path.join(root_dir, item)
    dst = os.path.join(archive_dir, item)
    try:
        if os.path.exists(dst):
            if os.path.isdir(dst): shutil.rmtree(dst)
            else: os.remove(dst)
        shutil.move(src, dst)
        print(f"Moved to archive: {item}")
    except Exception as e:
        print(f"Error: {e}")

print("Workspace cleanly archived!")
