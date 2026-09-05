"""Read-only pinned Halo 3 weapon palette/render-record diagnostic snapshot.

Run only during a diagnostic session with Halo 3 loaded. Unsynchronized reads
are observations, not a proof of what either headset eye displayed.
"""
import argparse
import ctypes as C
import importlib.util
import json
from pathlib import Path
import struct
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "out/python-deps"))


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(file))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sample-seconds", type=float, default=0,
                        help="observe transient matching records for up to 30 seconds")
    args = parser.parse_args()
    if not 0 <= args.sample_seconds <= 30:
        parser.error("sample-seconds must be between 0 and 30")
    engine = load("engine", "mcc-engine-control.py")
    verifier = load("skinning", "verify-h3-sword-skinning.py")
    proc = engine.only((p for p in engine.enumerate_items(2, 0, engine.Process, "Process32")
                        if p.exe.lower() in ("mcc-win64-shipping.exe", "mccwinstore-win64-shipping.exe")), "MCC process")
    module = engine.only((m for m in engine.enumerate_items(0x18, proc.pid, engine.Module, "Module32")
                          if m.name.lower() == "halo3.dll"), "Halo 3 module")
    identity = verifier.verify(Path(module.path))
    pe = engine.pefile.PE(str(module.path), fast_load=True)
    def rip(site, prefix):
        instruction = pe.get_data(site, len(prefix) + 4)
        if instruction[:len(prefix)] != prefix:
            raise ValueError(f"unexpected instruction at {site:#x}")
        return site + len(instruction) + struct.unpack_from("<i", instruction, len(prefix))[0]
    # Proven producer/consumer instructions in the skinning investigation.
    tag_base_rva = rip(0x26686D, b"\x4c\x8b\x15")
    instances_rva = rip(0x266877, b"\x4c\x8b\x05")
    draw_count_rva = rip(0x29591E, b"\x0f\xb7\x05")
    handle = engine.open_process(0x410, False, proc.pid)  # query + read only
    if not handle:
        raise C.WinError(C.get_last_error())
    try:
        def read(address, size):
            buffer = C.create_string_buffer(size)
            count = C.c_size_t()
            if not engine.read_memory(handle, address, buffer, size, C.byref(count)) or count.value != size:
                raise C.WinError(C.get_last_error())
            return buffer.raw
        def u64(address):
            return struct.unpack("<Q", read(address, 8))[0]
        base = module.base
        tag_base = u64(base + tag_base_rva)
        instances = u64(base + instances_rva)
        count = struct.unpack("<I", read(base + 0xA7AC24, 4))[0]
        if not 0 <= count <= 4:
            raise ValueError("weapon palette count outside native capacity")
        palette = read(base + 0xA7AC28, count * 0xD0C)
        entries = []
        for i in range(count):
            entry = palette[i*0xD0C:(i+1)*0xD0C]
            render, object_handle, metadata = struct.unpack_from("<III", entry)
            address = struct.unpack("<I", read(instances + (render & 0xFFFF)*8 + 4, 4))[0]
            if not address:
                continue
            definition = read(tag_base + address*4, 0x38)
            nodes, node_address = struct.unpack_from("<II", definition, 0x30)
            if not 1 <= nodes <= 64 or not node_address:
                continue
            entries.append({"slot": i, "render_tag": render, "object_handle": object_handle,
                            "metadata": metadata, "node_count": nodes,
                            "live_nodes": [struct.unpack_from("<13f", entry, 12+n*52) for n in range(nodes)],
                            "inverse_binds": [struct.unpack("<13f", read(tag_base+node_address*4+n*96+40, 52)) for n in range(nodes)]})
        draw_count = struct.unpack("<H", read(base + draw_count_rva, 2))[0]
        if draw_count > 1024:
            raise ValueError("draw count exceeds diagnostic read bound")
        draws = read(base + 0x91AC60, draw_count * 96)
        records = []
        for i in range(draw_count):
            record = draws[i*96:(i+1)*96]
            render = struct.unpack_from("<I", record, 4)[0]
            obj = struct.unpack_from("<I", record, 0x48)[0]
            if not any(e["render_tag"] == render and e["object_handle"] == obj for e in entries):
                continue
            regions = struct.unpack_from("<I", record)[0]
            if regions > 16:
                continue
            records.append({"slot": i, "render_tag": render, "object_handle": obj,
                            "region_count": regions, "skinning_count_field": record[12],
                            "region_mesh_indices": struct.unpack_from(f"<{regions}H", record, 14),
                            "flags": record[0x58]})
        stable = (palette == read(base + 0xA7AC28, len(palette)) and
                  count == struct.unpack("<I", read(base + 0xA7AC24, 4))[0] and
                  draws == read(base + 0x91AC60, len(draws)) and
                  draw_count == struct.unpack("<H", read(base + draw_count_rva, 2))[0])
        observations = {}
        attempts = 0
        start = time.monotonic()
        while time.monotonic() - start < args.sample_seconds:
            attempts += 1
            current_count = struct.unpack("<I", read(base + 0xA7AC24, 4))[0]
            if current_count > 4:
                raise ValueError("palette count changed outside native capacity")
            current_palette = read(base + 0xA7AC28, current_count * 0xD0C)
            identities = {struct.unpack_from("<II", current_palette, i*0xD0C)
                          for i in range(current_count)}
            current_draw_count = struct.unpack("<H", read(base + draw_count_rva, 2))[0]
            if current_draw_count > 1024:
                raise ValueError("draw count changed outside diagnostic bound")
            current_draws = read(base + 0x91AC60, current_draw_count * 96)
            for i in range(current_draw_count):
                row = current_draws[i*96:(i+1)*96]
                tag = struct.unpack_from("<I", row, 4)[0]
                obj = struct.unpack_from("<I", row, 0x48)[0]
                regions = struct.unpack_from("<I", row)[0]
                if (tag, obj) not in identities or regions > 16:
                    continue
                meshes = struct.unpack_from(f"<{regions}H", row, 14)
                key = (tag, obj, meshes, row[0x58])
                if key not in observations:
                    observations[key] = {"render_tag": tag, "object_handle": obj,
                                         "region_mesh_indices": meshes, "flags": row[0x58],
                                         "first_observed_seconds": time.monotonic()-start,
                                         "observations": 0}
                observations[key]["observations"] += 1
            time.sleep(.001)
        result = {"scope": __doc__, "module_identity": identity, "pid": proc.pid,
                  "unchanged_on_reread": stable, "atomic_snapshot": False,
                  "draw_count": draw_count,
                  "tag_only_draw_candidates": [
                      {"slot": i, "render_tag": struct.unpack_from("<I", draws, i*96+4)[0],
                       "object_handle": struct.unpack_from("<I", draws, i*96+0x48)[0]}
                      for i in range(draw_count)
                      if any(e["render_tag"] == struct.unpack_from("<I", draws, i*96+4)[0]
                             for e in entries)],
                  "draw_record_prefixes": [draws[i*96:i*96+16].hex()
                                           for i in range(min(draw_count, 8))],
                  "palette": entries, "matching_draw_records": records}
        result["sampling"] = {"requested_seconds": args.sample_seconds,
                              "attempts": attempts, "matches": list(observations.values()),
                              "atomic_snapshot": False}
        args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        print(json.dumps({"palette_entries": len(entries), "draw_records": len(records),
                          "unchanged_on_reread": stable, "sampling_attempts": attempts,
                          "sampled_distinct_matches": len(observations)}))
    finally:
        engine.close(handle)


if __name__ == "__main__":
    main()
