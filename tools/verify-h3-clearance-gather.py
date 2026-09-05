"""Verify the official movement-to-feature-gather match; read-only, no hook."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'out/pydeps'))
import pefile


def verify(official, retail):
    spec = importlib.util.spec_from_file_location(
        'point_evidence', Path(__file__).with_name('verify-h3-clearance-point.py'))
    point = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(point)
    # This first pins both input hashes and verifies the separate interior test.
    result = point.verify(official, retail)
    modules = {name: pefile.PE(str(path), fast_load=True)
               for name, path in [('official', official), ('retail', retail)]}
    calls = {'official': {0x64ED6A: 0x64D6E0, 0x64EDAD: 0x64EE60,
                          0x64DD5A: 0x6547D0},
             'retail': {0x1FFC58: 0x1FE800, 0x1FFC9B: 0x1FEF30,
                        0x1FE967: 0x25583C, 0x1FE9AC: 0x24BC10,
                        0x1FEE13: 0x1FE64C}}
    for label, edges in calls.items():
        for site, target in edges.items():
            ins = modules[label].get_data(site, 5)
            if ins[0] != 0xE8 or site + 5 + struct.unpack('<i', ins[1:])[0] != target:
                raise ValueError(f'{label}: gather call mismatch at {site:#x}')
    patterns = {
        0x1FEEE4: '66 45 39 34 24 75 10 66 45 39 74 24 02 75 08 66 45 39 74 24 04 74 03 41 b6 01',
        0x1FFC50: 'f3 0f 58 d6 f3 0f 58 d0 e8 a3 eb ff ff 84 c0 74 44',
    }
    for expected, pattern in patterns.items():
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
            raise ValueError(f'Nonunique gather signature: {matches}')
    for label, site in [('official', 0x64D740), ('retail', 0x1FE84D)]:
        ins = modules[label].get_data(site, 8)
        if ins[:4] != bytes.fromhex('f3 0f 58 3d'):
            raise ValueError('Radius inflation instruction mismatch')
        constant = site + 8 + struct.unpack('<i', ins[4:])[0]
        if struct.unpack('<f', modules[label].get_data(constant, 4))[0] != 0.0625:
            raise ValueError('Radius inflation constant mismatch')
    result.update({
        'gather_calls': {k: {hex(a): hex(b) for a, b in v.items()} for k, v in calls.items()},
        'gather_unique_patterns': {hex(a): b for a, b in patterns.items()},
        'retail_feature_gather_candidate': '0x1fe800',
        'native_radius_inflation_world_units': 0.0625,
        'limitations': result['limitations'] + [
            'Retail gather consumes the first ignored-object argument; do not assume two exclusions.',
            'Intermediate feature coverage, dynamic freshness and runtime cost remain unproven.']})
    return result


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
