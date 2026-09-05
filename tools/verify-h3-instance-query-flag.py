"""Verify the official and retail instance gate used by Halo 3 wall rays.

Read-only PE checks; no process attachment or hook installation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'out/pydeps'))
import pefile


def verify(official, retail):
    sources = {}
    for label, path, expected in (
        ('official', official, '59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602'),
        ('retail', retail, 'B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63')):
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest().upper()
        if digest != expected:
            raise ValueError(f'{label}: unverified module identity')
        sources[label] = pefile.PE(data=data, fast_load=True)
    retail_pe = sources['retail']
    pattern = bytes.fromhex('41 8b c0 40 32 f6 c1 e8 03 41 ba 01 00 00 00 49 8b d8 0f b7 fa 41 84 c2 0f 84 c8 01 00 00')
    matches = []
    for section in retail_pe.sections:
        if not section.Characteristics & 0x20000000:
            continue
        data = section.get_data()
        offset = 0
        while (offset := data.find(pattern, offset)) >= 0:
            matches.append(section.VirtualAddress + offset)
            offset += 1
    if matches != [0x1FE3D8]:
        raise ValueError(f'Instance gate is not uniquely verified: {matches}')
    for label, site, target in (
        ('official', 0x65364D, 0x651790),
        ('retail', 0x1FDD33, 0x1FE3B8),
        ('retail', 0x1FDE05, 0x1FE3B8)):
        code = sources[label].get_data(site, 5)
        if code[0] != 0xE8 or site + 5 + struct.unpack('<i', code[1:])[0] != target:
            raise ValueError(f'{label}: caller mismatch at {site:#x}')
    official_gate = sources['official'].get_data(0x6517BA, 3)
    if official_gate != bytes.fromhex('c1 e8 03'):
        raise ValueError('Official low-bit-three extraction differs')
    return {'retail_gate_rva': hex(matches[0]), 'required_low_flag': '0x8',
            'bsp_and_instance_flags': '0x9', 'all_object_flags_unchanged': '0x7fff00000000',
            'headset_acceptance': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--official', type=Path, required=True)
    parser.add_argument('--retail', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.official, args.retail)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
