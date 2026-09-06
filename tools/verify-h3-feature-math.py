"""Read-only audit of the pinned H3 first-contact math closure; no native calls."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'out/pydeps'))
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86_const import X86_OP_IMM, X86_OP_MEM, X86_REG_RIP


def verify(official, retail):
    spec = importlib.util.spec_from_file_location('volume', Path(__file__).with_name('verify-h3-volume-movement.py'))
    volume = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(volume)
    baseline = volume.verify(official, retail)  # Pins both input identities and matched first-hit ABI.
    pe = pefile.PE(str(retail))
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    # Full unwind ranges reviewed for argument/scratch-only pointer accesses.
    expected = {
        0x24B8B0: (0x24BC0D, 'BFEDE714CA2C887B517ADA99716DD074FB8D867696D570F3B51DA2A5144C6797', {0x24AFF0, 0x24B1F8, 0x24B5B8}),
        0x24AFF0: (0x24B1F7, 'F945E94778B13B2E0BCA2ADAAE3978D0763D3EE80528D231783A3FB5524600EE', {0x6F5612, 0x212AC}),
        0x24B1F8: (0x24B5B6, '3D1DE9A90F5E5BDF5223BA78BCD4C6B4F12CFFD91FE84C859495641E2A484C21', {0x6F5612, 0x212AC}),
        0x24B5B8: (0x24B8AF, '57794560C063BF666E3CA6487303DBA09B31EE093BD37C7B224A7AF5F4ED391C', set()),
        0x212AC: (0x21365, '5B16BB5F0AB87C2FE1DFE3F8F3D34DEF5623B380C681F4B999ABC914014C9C9D', {0x6F5612}),
    }
    unwind = {e.struct.BeginAddress: e.struct.EndAddress for e in pe.DIRECTORY_ENTRY_EXCEPTION}
    records = []
    for start, (end, expected_hash, expected_calls) in expected.items():
        if unwind.get(start) != end:
            raise ValueError(f'Unwind range mismatch at {start:x}')
        code = pe.get_data(start, end-start)
        digest = hashlib.sha256(code).hexdigest().upper()
        if digest != expected_hash:
            raise ValueError(f'Reviewed math body changed at {start:x}')
        instructions = list(md.disasm(code, start))
        if sum(i.size for i in instructions) != len(code):
            raise ValueError('Incomplete math-body disassembly')
        calls, references = set(), []
        for ins in instructions:
            if 'gs:' in ins.op_str or 'fs:' in ins.op_str:
                raise ValueError('Unexpected thread-local access')
            if ins.mnemonic == 'call' or ins.group(1):
                if ins.operands[0].type != X86_OP_IMM:
                    raise ValueError('Unexpected indirect control flow')
                target = ins.operands[0].imm
                if ins.mnemonic == 'call':
                    calls.add(target)
                elif not start <= target < end:
                    raise ValueError('Unreviewed external tail branch')
            for operand in ins.operands:
                if operand.type != X86_OP_MEM or operand.mem.base != X86_REG_RIP:
                    continue
                address = ins.address+ins.size+operand.mem.disp
                section = pe.get_section_by_rva(address)
                if not section or section.Characteristics & 0x80000000:
                    raise ValueError('Math closure references mutable module data')
                references.append({'instruction': hex(ins.address), 'rva': hex(address),
                    'section': section.Name.rstrip(b'\0').decode(),
                    'bytes': pe.get_data(address, max(operand.size, 16)).hex()})
        if calls != expected_calls:
            raise ValueError(f'Math call closure changed at {start:x}')
        records.append({'start': hex(start), 'end': hex(end), 'sha256': digest,
            'callees': [hex(x) for x in sorted(calls)], 'read_only_references': references})
    thunk = pe.get_data(0x6F5612, 6)
    if thunk[:2] != b'\xff\x25':
        raise ValueError('Math import thunk changed')
    iat = 0x6F5618+struct.unpack('<i', thunk[2:])[0]
    matches = [(d.dll.decode(), i.name.decode() if i.name else '')
        for d in pe.DIRECTORY_ENTRY_IMPORT for i in d.imports
        if i.address-pe.OPTIONAL_HEADER.ImageBase == iat]
    if matches != [('api-ms-win-crt-math-l1-1-0.dll', 'sqrtf')]:
        raise ValueError(f'Unreviewed math import: {matches}')
    expected_axes = (2, 1, 0, 1, 2, 0, 0, 2, 1, 2, 0, 1, 1, 0, 2, 0, 1, 2)
    # Official 71C6A7 and retail 24B749 address the same six rows. Both
    # helpers form row=projection*2+orientation and use its first two axes.
    official_pe = pefile.PE(str(official), fast_load=True)
    projection_tables = {}
    for label, image, rva in [('official', official_pe, 0xFEFC68), ('retail', pe, 0x768EF0)]:
        table = struct.unpack('<18h', image.get_data(rva, 36))
        if table != expected_axes:
            raise ValueError(f'{label}: prism projection table changed')
        projection_tables[label] = {'rva': hex(rva), 'rows': [table[i:i+3] for i in range(0,18,3)]}
    return {'module_hashes': baseline['module_hashes'], 'functions': records,
        'prism_projection_tables': projection_tables,
        'only_external_callee': {'thunk': '0x6F5612', 'iat': hex(iat), 'import': matches[0]},
        'interpretation': 'Reviewed closure uses supplied feature storage, points/vectors, result storage, local scratch and read-only constants; no object lookup, world gather, engine TLS, allocation or logging calls.',
        'limits': ['This is pinned-code evidence, not a render-thread runtime test.',
                   'Callers must supply immutable complete feature snapshots with valid nested counts and indices.',
                   'Cached geometry must cover the entire requested sweep and match its radius; scene lifetime and starting clearance are separate proofs.']}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--official', type=Path, required=True)
    p.add_argument('--retail', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    result = verify(a.official, a.retail)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
