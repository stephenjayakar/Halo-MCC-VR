"""Read-only official-to-retail point-query evidence; never installs a binding."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'out/pydeps'))
import pefile


def verify(official, retail):
    modules = {}
    hashes = {}
    for label, path, expected in (
        ('official', official, '59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602'),
        ('retail', retail, 'B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63')):
        data = path.read_bytes()
        hashes[label] = hashlib.sha256(data).hexdigest().upper()
        if hashes[label] != expected:
            raise ValueError(f'{label}: unverified module identity')
        modules[label] = pefile.PE(data=data, fast_load=True)
    calls = {
        'official': {0x65209D: 0x6520B0, 0x652177: 0x2D36C0,
                     0x6521AA: 0x4AB6F0, 0x65236C: 0x654220},
        'retail': {0x1FCB65: 0x16A4C4, 0x1FCCA5: 0x1FC79C,
                   0x1FC8B2: 0x16A4C4, 0x1FCDC1: 0x1FC8E0},
    }
    for label, edges in calls.items():
        for site, target in edges.items():
            ins = modules[label].get_data(site, 5)
            if ins[0] != 0xE8 or site + 5 + struct.unpack('<i', ins[1:])[0] != target:
                raise ValueError(f'{label}: call mismatch at {site:#x}')
    signatures = {
        0x16A53F: '44 0f 43 c8 41 8b c1 c1 e8 17 f7 d0 a8 01 75 a5 41 83 f9 ff 74 05 44 23 cf eb 04',
        0x1FCB65: 'e8 5a d9 f6 ff 41 3b c5 75 16 41 b6 01 41 bc 01 00 00 00',
    }
    for expected, pattern in signatures.items():
        pattern = bytes.fromhex(pattern)
        matches = []
        for section in modules['retail'].sections:
            if not section.Characteristics & 0x20000000:
                continue
            data = section.get_data()
            offset = 0
            while (offset := data.find(pattern, offset)) >= 0:
                matches.append(section.VirtualAddress + offset)
                offset += 1
        if matches != [expected]:
            raise ValueError(f'Retail signature not unique: {matches}')
    # The official assertion names the structure flag; do not infer its name.
    pe = modules['official']
    site = 0x652138
    ins = pe.get_data(site, 7)
    if ins[:3] != bytes.fromhex('48 8d 0d'):
        raise ValueError('Official assertion instruction mismatch')
    record = site + 7 + struct.unpack('<i', ins[3:])[0]
    pointer = struct.unpack('<Q', pe.get_data(record, 8))[0]
    expression = pe.get_data(pointer - pe.OPTIONAL_HEADER.ImageBase, 128).split(b'\0')[0].decode()
    if expression != 'TEST_FLAG(flags.collision_flags, _collision_test_structure_bit)':
        raise ValueError('Official structure assertion mismatch')
    return {'module_hashes': hashes,
            'calls': {label: {hex(a): hex(b) for a, b in edges.items()}
                      for label, edges in calls.items()},
            'unique_retail_signatures': {hex(a): b for a, b in signatures.items()},
            'official_assertion': expression,
            'retail_point_core_candidate': '0x1fcac0',
            'retail_leaf_traversal': '0x16a4c4',
            'runtime_invoked': False, 'whole_weapon_clearance_proven': False,
            'limitations': ['No active structure can return no hit.',
                            'Point classification does not certify a swept volume.',
                            'Complete query ABI and geometry exclusions still require review.']}


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
