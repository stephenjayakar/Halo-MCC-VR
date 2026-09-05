"""Preserve read-only official H3EK evidence for a possible clearance query.

This does not install a hook or establish a retail binding. Requires pefile
and capstone (the workspace copies under out/pydeps are supported).
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'out/pydeps'))
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

PINNED_SHA = '59A78F2C96034D7CEB5D710505B2B36813AA141FC81A083E3F952973DBCE4602'


def audit(path, output):
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest().upper()
    if digest != PINNED_SHA:
        raise ValueError('Unverified H3EK identity; no address interpretation allowed')
    pe = pefile.PE(data=data, fast_load=True)
    calls = {0x5AECA6: 0x652090, 0x65209D: 0x6520B0,
             0x64ED6A: 0x64D6E0, 0x64EDAD: 0x64EE60,
             0x64D791: 0x71B590, 0x64E03B: 0x64D6E0}
    for site, target in calls.items():
        instruction = pe.get_data(site, 5)
        if instruction[0] != 0xE8 or site + 5 + struct.unpack('<i', instruction[1:])[0] != target:
            raise ValueError(f'Call mismatch at {site:#x}')
    assertions = {}
    for site, expected in ((0x64DEDF, 'center'), (0x64DF51, 'new_center'),
                           (0x64DF62, 'new_radius')):
        ins = pe.get_data(site, 7)
        if ins[:3] != bytes.fromhex('48 8d 0d'):
            raise ValueError(f'Assertion reference mismatch at {site:#x}')
        record = site + 7 + struct.unpack('<i', ins[3:])[0]
        pointer = struct.unpack('<Q', pe.get_data(record, 8))[0]
        expression = pe.get_data(pointer - pe.OPTIONAL_HEADER.ImageBase, 128).split(b'\0')[0].decode('ascii')
        if expression != expected:
            raise ValueError(f'Assertion expression mismatch at {site:#x}')
        assertions[hex(site)] = expression
    output.mkdir(parents=True, exist_ok=True)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    ranges = {'point_spawn_caller': (0x5AEC60, 0x5AED1B),
              'point_wrapper': (0x652090, 0x652144),
              'feature_gather': (0x64D6E0, 0x64DE5C),
              'feature_initialize': (0x71B590, 0x71B59B),
              'sphere_adjustment': (0x64DE60, 0x64EC20),
              'movement_caller': (0x64EC20, 0x64EE60),
              'feature_solver_entry': (0x64EE60, 0x64F00A)}
    for name, (start, end) in ranges.items():
        lines = [f'{i.address:08x}  {i.mnemonic} {i.op_str}'
                 for i in md.disasm(pe.get_data(start, end - start), start)]
        (output / (name + '.txt')).write_text('\n'.join(lines) + '\n')
    result = {'source': str(path.resolve()), 'sha256': digest,
              'verified_calls': {hex(k): hex(v) for k, v in calls.items()},
              'verified_assertions': assertions,
              'retail_binding_verified': False, 'runtime_tested': False,
              'limitations': ['Names other than assertion text are descriptive hypotheses.',
                              'Feature overflow, disabled flags, interior solids, dynamic freshness and cost remain unaudited.',
                              'A zero feature count is not yet a certified free-space volume.']}
    (output / 'audit.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(audit(args.module, args.output), indent=2))
