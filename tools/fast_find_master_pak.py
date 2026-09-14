import struct
import zlib
import os
from grit_datapack import ReadDataPack

raw_img = r'D:\openfyde\keshos_clean.img'
known_asset_offset = 2102081465

print('Fast scanning for valid Master PAK header...')
with open(raw_img, 'rb') as f:
    # Scan from 2070 MB to 2102 MB
    start_search = 2070 * 1024 * 1024
    f.seek(start_search)
    buf = f.read(35 * 1024 * 1024)
    
    pos = 0
    hdr = b'\x05\x00\x00\x00\x01\x00\x00\x00'
    while True:
        idx = buf.find(hdr, pos)
        if idx == -1:
            break
        
        abs_offset = start_search + idx
        num_entries, alias_count = struct.unpack('<HH', buf[idx+8:idx+12])
        
        if 500 < num_entries < 25000:
            # Check if entries table has strictly ascending IDs
            valid_table = True
            first_entries = []
            for i in range(min(15, num_entries)):
                entry_pos = idx + 12 + i * 6
                r_id, r_off = struct.unpack('<HI', buf[entry_pos:entry_pos+6])
                first_entries.append((r_id, r_off))
                if i > 0 and r_id <= first_entries[i-1][0]:
                    valid_table = False
                    break
            
            if valid_table:
                print(f'*** Valid Master PAK v5 found at absolute offset: {abs_offset} (entries: {num_entries}) ***')
                print('   First 5 entries:', first_entries[:5])
                
                # Check if this pak encompasses known_asset_offset (2102081465)
                f.seek(abs_offset)
                pak_data = f.read(150 * 1024 * 1024)
                dp = ReadDataPack(pak_data)
                print(f'   Successfully parsed DataPack with {len(dp.resources)} resources!')
                
                # Search for welcome assets inside this DataPack
                welcome_ids = {}
                for rid, rdata in dp.resources.items():
                    try:
                        dec = zlib.decompress(rdata, 16 + zlib.MAX_WBITS)
                    except Exception:
                        dec = rdata
                    
                    if b'oobe-welcome' in dec or b'fydeos_v22_welcome' in dec:
                        welcome_ids[rid] = len(dec)
                        print(f'   -> Match Resource ID {rid}: decompressed length {len(dec)} bytes')
                
                print(f'   Total matched OOBE resources: {len(welcome_ids)}')
                break
        
        pos = idx + 4

print('Done.')
