"""
Pure Python ISO9660 Extractor to extract files from Gentoo minimal ISO without external tools.
"""
import os, struct

iso_path = r'c:\Users\DanDevXP\Desktop\keshoos\gentoo_kernel\gentoo_minimal.iso'
dest_dir = r'c:\Users\DanDevXP\Desktop\keshoos\gentoo_kernel'

SECTOR_SIZE = 2048

def parse_dir_record(data, offset):
    rec_len = data[offset]
    if rec_len == 0: return None, offset + 1
    ext_loc = struct.unpack('<I', data[offset+2:offset+6])[0]
    data_len = struct.unpack('<I', data[offset+10:offset+14])[0]
    flags = data[offset+25]
    name_len = data[offset+32]
    name = data[offset+33:offset+33+name_len].decode('latin1', errors='ignore').split(';')[0]
    is_dir = bool(flags & 2)
    return (name, ext_loc, data_len, is_dir), offset + rec_len

with open(iso_path, 'rb') as f:
    # 1. Read Primary Volume Descriptor at sector 16
    f.seek(16 * SECTOR_SIZE)
    pvd = f.read(SECTOR_SIZE)
    if pvd[1:6] != b'CD001':
        print("Error: Not a valid ISO9660 image!")
        exit(1)
    
    root_rec, _ = parse_dir_record(pvd, 156)
    root_loc, root_len = root_rec[1], root_rec[2]
    print(f"Root Directory at sector {root_loc} (length {root_len})")
    
    # 2. Read Root Directory
    f.seek(root_loc * SECTOR_SIZE)
    root_data = f.read(root_len)
    
    files_to_extract = {}
    
    offset = 0
    while offset < root_len:
        rec, offset = parse_dir_record(root_data, offset)
        if not rec:
            # Advance to next sector boundary
            offset = ((offset + SECTOR_SIZE - 1) // SECTOR_SIZE) * SECTOR_SIZE
            continue
        name, loc, length, is_dir = rec
        if name and name not in ('\x00', '\x01'):
            print(f"  [Root Entry] {name} ({'DIR' if is_dir else f'{length} bytes'}, sector {loc})")
            if is_dir and name.upper() == 'BOOT':
                # Read /boot directory
                f_pos = f.tell()
                f.seek(loc * SECTOR_SIZE)
                boot_data = f.read(length)
                f.seek(f_pos)
                b_off = 0
                while b_off < length:
                    brec, b_off = parse_dir_record(boot_data, b_off)
                    if not brec:
                        b_off = ((b_off + SECTOR_SIZE - 1) // SECTOR_SIZE) * SECTOR_SIZE
                        continue
                    b_name, b_loc, b_len, b_is_dir = brec
                    if b_name and b_name not in ('\x00', '\x01'):
                        print(f"    [/boot Entry] {b_name} ({b_len} bytes, sector {b_loc})")
                        files_to_extract[b_name] = (b_loc, b_len)
            elif not is_dir:
                files_to_extract[name] = (loc, length)

    # 3. Extract requested files
    print("\nExtracting official Gentoo files...")
    for target, (floc, flen) in files_to_extract.items():
        if target.upper() in ('GENTOO', 'GENTOO.IGZ', 'IMAGE.SQUASHFS') or target.startswith('gentoo'):
            out_file = os.path.join(dest_dir, target)
            print(f"  [+] Extracting {target} ({flen / (1024*1024):.2f} MB)...")
            f.seek(floc * SECTOR_SIZE)
            with open(out_file, 'wb') as out_f:
                left = flen
                while left > 0:
                    chunk = f.read(min(32*1024*1024, left))
                    if not chunk: break
                    out_f.write(chunk)
                    left -= len(chunk)
            print(f"  [DONE] Saved: {out_file}")

print("[SUCCESS] Pure Python ISO9660 extraction complete!")
