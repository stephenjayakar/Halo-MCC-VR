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
            0x64F1FB: ('call', '0x71bcc0'),
            0x64F22E: ('subss', 'xmm0, dword ptr [rbx + 0x10]'),
            0x71BCED: ('mov', 'r15, rdx'),
            0x71BCF4: ('mov', 'r12, r9'),
            0x71BCFC: ('mov', 'rdi, r8'),
            0x71BFDA: ('comiss', 'xmm15, xmm6'),
            0x71BFDE: ('jbe', '0x71c022'),
            0x71C057: ('movss', 'dword ptr [r12 + 0x10], xmm15'),
            0x71C062: ('mulss', 'xmm0, dword ptr [rdi]'),
            0x71C06A: ('addss', 'xmm0, dword ptr [r15]'),
            0x71C06F: ('movss', 'dword ptr [r12 + 0x14], xmm0'),
            0x71C09B: ('movups', 'xmmword ptr [r12 + 0x20], xmm2'),
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
            0x1FF149: ('subss', 'xmm0, dword ptr [rdi + 0x10]'),
            0x24B8D6: ('mov', 'r14, r8'),
            0x24B8E1: ('mov', 'r15, rdx'),
            0x24B8E8: ('mov', 'rdi, rcx'),
            0x24B8EF: ('mov', 'rbx, r9'),
            0x24BAC5: ('comiss', 'xmm6, dword ptr [rbp - 0x4c]'),
            0x24BAC9: ('jbe', '0x24b9b8'),
            0x24B9EC: ('movss', 'dword ptr [rbx + 0x10], xmm6'),
            0x24B9F4: ('mulss', 'xmm0, dword ptr [r14]'),
            0x24B9FF: ('addss', 'xmm0, dword ptr [r15]'),
            0x24BA04: ('movss', 'dword ptr [rbx + 0x14], xmm0'),
            0x24BA2B: ('movdqu', 'xmmword ptr [rbx + 0x20], xmm7'),
            0x24BBA3: ('mov', 'dword ptr [rbx + 0x10], 0x3f800000'),
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
                       (0x64EFD3, '_collision_feature_prism'),
                       (0x64F3DE, '&collision->plane')]:
        ins = official_pe.get_data(site, 7)
        if ins[:3] != bytes.fromhex('4c 8d 05' if site == 0x64F3DE else '48 8d 0d'):
            raise ValueError('Assertion operand mismatch')
        record = site + 7 + struct.unpack('<i', ins[3:])[0]
        if site == 0x64F3DE:
            text = official_pe.get_data(record, 200).split(b'\0')[0].decode('ascii')
        else:
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
    first_hit = images['retail'].get_data(0x24B8B0, 0x34)
    first_hit_matches = []
    for section in images['retail'].sections:
        if not section.Characteristics & 0x20000000: continue
        code = section.get_data(); start = 0
        while (start := code.find(first_hit, start)) >= 0:
            first_hit_matches.append(section.VirtualAddress + start); start += 1
    if first_hit_matches != [0x24B8B0]: raise ValueError(f'Nonunique first-hit entry: {first_hit_matches}')
    return {
        'module_hashes': baseline['module_hashes'],
        'checked_instructions': {k: {hex(s): ' '.join(v) for s, v in sites.items()} for k, sites in expected.items()},
        'official_assertions': assertions,
        'retail_solver_rva': '0x1FEF30', 'unique_solver_signature': solver.hex(' ').upper(),
        'solver_call_shape': 'position*, displacement*, gathered_features*, new_position*, new_velocity*, uint16 maximum_count, records*',
        'record_stride': 48,
        'first_hit_query': {
            'official_rva': '0x71BCC0', 'retail_rva': '0x24B8B0',
            'unique_signature': first_hit.hex(' ').upper(),
            'call_shape': 'bool(features*, start*, displacement*, collision_record*)',
            'record_fields': {'0x10': 'first fraction', '0x14': 'start + displacement * fraction',
                              '0x20': 'contact plane: normal xyz and distance'},
            'runtime_tested_directly': False,
            'limitations': ['First inward-facing contact only; not an initial-overlap or interior recovery query.',
                            'Record plane is unspecified when return is false.']
        },
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
