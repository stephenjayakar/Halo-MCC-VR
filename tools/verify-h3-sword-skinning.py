#!/usr/bin/env python3
"""Read-only verification of the pinned retail visible-palette skinning chain.

Requires pefile. This is offline evidence, not a runtime hook installer.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import pefile


def verify(path):
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest().upper()
    if digest != "B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63":
        raise ValueError("unverified retail module identity")
    pe = pefile.PE(data=data, fast_load=True)
    # Structural sequences selected after tracing the H3EK consumer and retail
    # palette buffer. Uniqueness is checked across every executable section.
    signatures = {
        "node_and_inverse_arguments": (0x266990,
            "49 63 CD 4C 8D 45 00 48 8D 14 49 48 C1 E2 05 48 6B C9 34 "
            "48 83 C2 28 49 03 CC 49 03 D7"),
        "visible_palette_to_skinning": (0x295BA9,
            "41 8B 14 24 4D 8D 44 24 0C 48 89 4C 24 38 BE 01 00 00 00 "
            "41 8B 4C 24 04 4C 8B CF 40 88 74 24 28"),
    }
    matches = {}
    for name, (expected, pattern) in signatures.items():
        needle = bytes.fromhex(pattern)
        found = []
        for section in pe.sections:
            if not section.Characteristics & 0x20000000:
                continue
            payload = section.get_data()
            start = 0
            while (offset := payload.find(needle, start)) >= 0:
                found.append(section.VirtualAddress + offset)
                start = offset + 1
        if found != [expected]:
            raise ValueError(f"{name}: expected one verified match, got {found}")
        matches[name] = hex(found[0])
    calls = {0x28AE53: 0x2C0D20, 0x2C0F39: 0x2C5A38,
             0x2C5A74: 0x2C561C, 0x295BC9: 0x266838,
             0x2669AD: 0x120DF8}
    for site, target in calls.items():
        instruction = pe.get_data(site, 5)
        if instruction[0] != 0xE8 or site + 5 + struct.unpack_from("<i", instruction, 1)[0] != target:
            raise ValueError(f"call target differs at {site:#x}")
    # Both sides explicitly address the same first-person palette array.
    for site in (0x28AE4C, 0x2958A7):
        instruction = pe.get_data(site, 7)
        if instruction[:3] not in (b"\x4c\x8d\x0d", b"\x4c\x8d\x25"):
            raise ValueError("unexpected palette reference instruction")
        if site + 7 + struct.unpack_from("<i", instruction, 3)[0] != 0xA7AC28:
            raise ValueError("palette producer and consumer differ")
    return {"schema": 1, "module": str(path.resolve()), "sha256": digest,
            "scope": "offline pinned-module verification; no runtime acceptance",
            "unique_sequences": matches,
            "verified_calls": {hex(k): hex(v) for k, v in calls.items()},
            "palette_array_rva": "0xa7ac28", "palette_node_offset": 12,
            "live_node_stride": 52, "render_node_stride": 96,
            "inverse_bind_offset": 40, "shader_matrix_stride": 48}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.module)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
