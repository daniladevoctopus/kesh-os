"""
Move old bloated project files into archive directory,
preserving tools, glaze_ui, keshos_oobe, logos and master documents.
"""
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

items = os.listdir(root_dir)
for item in items:
    if item in keep_list:
        continue
    src_path = os.path.join(root_dir, item)
    dst_path = os.path.join(archive_dir, item)
    try:
        if os.path.exists(dst_path):
            if os.path.isdir(dst_path): shutil.rmtree(dst_path)
            else: os.remove(dst_path)
        shutil.move(src_path, dst_path)
        print(f"Archived: {item}")
    except Exception as e:
        print(f"Error archiving {item}: {e}")

print("[DONE] Old project contents successfully moved to archive/!")
