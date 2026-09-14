"""
Official Chromium DataPack v4 / v5 format encoder and decoder.
Based on Chromium's tools/grit/grit/format/data_pack.py
"""

import struct
import collections

BINARY = 0
UTF8 = 1
UTF16 = 2

DataPack = collections.namedtuple('DataPack', ['resources', 'encoding', 'version', 'aliases'])

def ReadDataPack(input_file_or_bytes):
    """Reads a Chromium .pak file and returns a DataPack(resources_dict, encoding, version, aliases_dict)."""
    if isinstance(input_file_or_bytes, bytes):
        data = input_file_or_bytes
    else:
        with open(input_file_or_bytes, 'rb') as f:
            data = f.read()

    if len(data) < 12:
        raise ValueError('Data too short for PAK header')

    version = struct.unpack('<I', data[:4])[0]
    
    if version == 4:
        # v4: uint32 version, uint32 num_entries, uint8 encoding
        num_entries, encoding = struct.unpack('<IB', data[4:9])
        header_len = 9
        alias_count = 0
    elif version == 5:
        # v5: uint32 version, uint32 encoding, uint16 resource_count, uint16 alias_count
        encoding, num_entries, alias_count = struct.unpack('<IHH', data[4:12])
        header_len = 12
    else:
        raise ValueError(f'Unsupported PAK version: {version}')

    # Read entries table
    resources = {}
    entry_size = 6
    entries = []
    
    for i in range(num_entries + 1):
        pos = header_len + i * entry_size
        r_id, r_offset = struct.unpack('<HI', data[pos:pos+entry_size])
        entries.append((r_id, r_offset))

    # Read aliases table (v5 only)
    aliases = {}
    if version == 5 and alias_count > 0:
        alias_table_pos = header_len + (num_entries + 1) * entry_size
        for i in range(alias_count):
            pos = alias_table_pos + i * 4
            a_id, a_idx = struct.unpack('<HH', data[pos:pos+4])
            if a_idx < num_entries:
                target_id = entries[a_idx][0]
                aliases[a_id] = target_id

    # Read resource payloads
    for i in range(num_entries):
        r_id, r_offset = entries[i]
        next_offset = entries[i+1][1]
        payload = data[r_offset:next_offset]
        resources[r_id] = payload

    # Map aliases to payloads
    for a_id, target_id in aliases.items():
        if target_id in resources:
            resources[a_id] = resources[target_id]

    return DataPack(resources=resources, encoding=encoding, version=version, aliases=aliases)

def WriteDataPack(resources, output_file, encoding=UTF8, version=5):
    """Writes a dictionary of {resource_id (int): data (bytes)} to a valid Chromium .pak file."""
    # Find unique payloads to optimize and create aliases if payloads match
    payload_to_id = {}
    unique_resources = {}
    aliases = {}

    # Sort all resource IDs
    sorted_ids = sorted(resources.keys())
    
    for r_id in sorted_ids:
        raw_val = resources[r_id]
        payload = raw_val.encode('utf-8') if isinstance(raw_val, str) else raw_val
        
        # Deduplicate to aliases if duplicate payload
        if payload in payload_to_id:
            aliases[r_id] = payload_to_id[payload]
        else:
            payload_to_id[payload] = r_id
            unique_resources[r_id] = payload

    unique_ids = sorted(unique_resources.keys())
    id_to_index = {r_id: idx for idx, r_id in enumerate(unique_ids)}
    
    num_entries = len(unique_ids)
    alias_count = len(aliases) if version == 5 else 0

    header_len = 12 if version == 5 else 9
    entry_size = 6
    index_len = (num_entries + 1) * entry_size
    alias_len = alias_count * 4 if version == 5 else 0
    data_start = header_len + index_len + alias_len

    if version == 5:
        header = struct.pack('<IIHH', version, encoding, num_entries, alias_count)
    else:
        header = struct.pack('<IIB', version, num_entries, encoding)

    # Build index and data
    index_entries = []
    data_blocks = []
    current_offset = data_start

    for r_id in unique_ids:
        payload = unique_resources[r_id]
        index_entries.append(struct.pack('<HI', r_id, current_offset))
        data_blocks.append(payload)
        current_offset += len(payload)

    # Final dummy entry
    index_entries.append(struct.pack('<HI', 0, current_offset))

    # Build alias table
    alias_entries = []
    if version == 5 and alias_count > 0:
        for a_id in sorted(aliases.keys()):
            target_id = aliases[a_id]
            target_index = id_to_index[target_id]
            alias_entries.append(struct.pack('<HH', a_id, target_index))

    with open(output_file, 'wb') as f:
        f.write(header)
        for ie in index_entries:
            f.write(ie)
        for ae in alias_entries:
            f.write(ae)
        for db in data_blocks:
            f.write(db)

    print(f'[SUCCESS] Wrote valid Chromium DataPack v{version} to {output_file} ({num_entries} unique, {alias_count} aliases, total {current_offset:,} bytes).')
