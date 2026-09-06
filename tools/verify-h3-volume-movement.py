"""Verify H3 native swept-volume building blocks, without installing or calling them."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'out/pydeps'))
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


def verify(official, retail):
    spec = importlib.util.spec_from_file_location('gather', Path(__file__).with_name('verify-h3-clearance-gather.py'))
    gather = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gather)
    baseline = gather.verify(official, retail)  # Pins identities and gather ABI.
    images = {k: pefile.PE(str(p), fast_load=True) for k, p in [('official', official), ('retail', retail)]}
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    expected = {
        'official': {
            0x65070E: ('movss', 'dword ptr [rsp + 0x20], xmm3'),
            0x650714: ('xorps', 'xmm3, xmm3'),
            0x650717: ('call', '0x64ec20'),
            0x64EDAD: ('call', '0x64ee60'),
            0x64EFE8: ('mov', 'r15, qword ptr [rbp + 0x210]'),
            0x64EFEF: ('movzx', 'eax, word ptr [rbp + 0x208]'),
            0x6502F7: ('lea', 'rsi, [rax + rax*2]'),
            0x6502FB: ('shl', 'rsi, 4'),
        },
        'retail': {
            0x1FFC45: ('or', 'dword ptr [rsp + 0x28], 0xffffffff'),
            0x1FFC4A: ('movss', 'dword ptr [rsp + 0x20], xmm0'),
            0x1FFC58: ('call', '0x1fe800'),
            0x1FFC61: ('mov', 'rax, qword ptr [rsp + 0xc4f0]'),
            0x1FFC6E: ('mov', 'r9, qword ptr [rsp + 0xc4d8]'),
            0x1FFC81: ('movzx', 'eax, word ptr [rsp + 0xc4e8]'),
            0x1FFC8E: ('mov', 'rax, qword ptr [rsp + 0xc4e0]'),
            0x1FFC9B: ('call', '0x1fef30'),
            0x1FEF5F: ('mov', 'rsi, qword ptr [rbp + 0x100]'),
            0x1FF0EC: ('lea', 'rdi, [rax + rax*2]'),
            0x1FF0F0: ('shl', 'rdi, 4'),
            0x1FF0FA: ('call', '0x24b8b0'),
        }
    }
    for label, sites in expected.items():
        for site, pair in sites.items():
            ins = next(md.disasm(images[label].get_data(site, 15), site))
            if (ins.mnemonic, ins.op_str) != pair:
                raise ValueError(f'{label} instruction mismatch at {site:x}: {ins.mnemonic} {ins.op_str}')
    # Source assertion record names the same three gathered feature categories.
    official_pe = images['official']
    assertions = {}
    for site, name in [(0x64EFAD, '_collision_feature_sphere'),
                       (0x64EFC0, '_collision_feature_cylinder'),
                       (0x64EFD3, '_collision_feature_prism')]:
        ins = official_pe.get_data(site, 7)
        if ins[:3] != bytes.fromhex('48 8d 0d'): raise ValueError('Assertion operand mismatch')
        record = site + 7 + struct.unpack('<i', ins[3:])[0]
        va = struct.unpack('<Q', official_pe.get_data(record, 8))[0]
        text = official_pe.get_data(va - official_pe.OPTIONAL_HEADER.ImageBase, 200).split(b'\0')[0].decode('ascii')
        if name not in text: raise ValueError('Assertion name mismatch')
        assertions[hex(site)] = text
    solver = images['retail'].get_data(0x1FEF30, 0x21)
    hits = []
    for section in images['retail'].sections:
        if not section.Characteristics & 0x20000000: continue
        code = section.get_data(); start = 0
        while (start := code.find(solver, start)) >= 0:
            hits.append(section.VirtualAddress + start); start += 1
    if hits != [0x1FEF30]: raise ValueError(f'Nonunique solver entry: {hits}')
    return {
        'module_hashes': baseline['module_hashes'],
        'checked_instructions': {k: {hex(s): ' '.join(v) for s, v in sites.items()} for k, sites in expected.items()},
        'official_assertions': assertions,
        'retail_solver_rva': '0x1FEF30', 'unique_solver_signature': solver.hex(' ').upper(),
        'solver_call_shape': 'position*, displacement*, gathered_features*, new_position*, new_velocity*, uint16 maximum_count, records*',
        'record_stride': 48,
        'retail_wrapper_exclusion': 'Movement wrapper forces gather ignored-object to -1. Use separately verified gather with explicit exclusion before solver for queries near the local player.',
        'player_movement_connection_proven': False,
        'runtime_tested': False,
        'limitations': ['Interior starts, swept-radius output and cost require controlled native probes.', 'This does not prove an arbitrary rotating weapon volume or full rendered-geometry coverage.']
    }


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--official', type=Path, required=True)
    p.add_argument('--retail', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    result = verify(a.official, a.retail)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
