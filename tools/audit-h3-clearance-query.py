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
             0x64D791: 0x71B590, 0x64E03B: 0x64D6E0,
             0x71A068: 0x71AF10, 0x719E13: 0x71B0E0,
             0x719976: 0x71A950}
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
    # Check the actual capacity and exclusion branches, not descriptive names.
    expected_instructions = {
        0x64D760: ('and', 'edi, 0xfffffff7'),
        0x64D779: ('and', 'dword ptr [rbp + 0x38f4], 0xfffffffe'),
        0x71AF2F: ('mov', 'r12d, 0xff'),
        0x71AF5F: ('cmp', 'r10w, r12w'),
        0x71AF63: ('ja', '0x71afb0'),
        0x71AF6D: ('mov', 'word ptr [rdx], r10w'),
        0x71B11C: ('mov', 'eax, 0x100'),
        0x71B12A: ('jge', '0x71b399'),
        0x71B155: ('mov', 'word ptr [rdx + 4], r9w'),
        0x71A9B5: ('mov', 'eax, 0xff'),
        0x71A9C2: ('ja', '0x71aa53'),
        0x71A9D7: ('mov', 'word ptr [rdi + 2], r8w'),
    }
    for site, expected in expected_instructions.items():
        ins = next(md.disasm(pe.get_data(site, 15), site), None)
        if not ins or (ins.mnemonic, ins.op_str) != expected:
            raise ValueError(f'Coverage/capacity instruction mismatch at {site:#x}')
    ranges = {'point_spawn_caller': (0x5AEC60, 0x5AED1B),
              'point_wrapper': (0x652090, 0x652144),
              'point_core': (0x6520B0, 0x652660),
              'point_leaf': (0x4AB6F0, 0x4AB880),
              'bsp_active': (0x2D36C0, 0x2D36D6),
              'feature_gather': (0x64D6E0, 0x64DE5C),
              'feature_initialize': (0x71B590, 0x71B59B),
              'sphere_adjustment': (0x64DE60, 0x64EC20),
              'movement_caller': (0x64EC20, 0x64EE60),
              'feature_solver_entry': (0x64EE60, 0x64F00A),
              'append_vertex': (0x719F90, 0x71A090),
              'append_surface': (0x719CF0, 0x719E40),
              'store_vertex': (0x71AF10, 0x71B0E0),
              'store_surface': (0x71B0E0, 0x71B3B0)}
    ranges.update({'append_edge': (0x7196E0, 0x7199E0),
                   'store_edge': (0x71A950, 0x71AF10)})
    for name, (start, end) in ranges.items():
        lines = [f'{i.address:08x}  {i.mnemonic} {i.op_str}'
                 for i in md.disasm(pe.get_data(start, end - start), start)]
        (output / (name + '.txt')).write_text('\n'.join(lines) + '\n')
    result = {'source': str(path.resolve()), 'sha256': digest,
              'verified_calls': {hex(k): hex(v) for k, v in calls.items()},
              'verified_assertions': assertions,
              'verified_instructions': {hex(k): ' '.join(v)
                                        for k, v in expected_instructions.items()},
              'retail_binding_verified': False, 'runtime_tested': False,
              'limitations': ['Names other than assertion text are descriptive hypotheses.',
                              'Base vertex/edge/surface output capacity guards are checked; intermediate collection and auxiliary extrusion paths remain unaudited.',
                              'Global switches can clear instance/object flags; retail switch state and complete coverage remain unverified.',
                              'Interior solids, dynamic freshness and cost remain unaudited.',
                              'A zero feature count is not yet a certified free-space volume.']}
    (output / 'audit.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(audit(args.module, args.output), indent=2))
