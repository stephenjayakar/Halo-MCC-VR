"""Read-only Halo 3 object snapshot through live thread TLS.

Uses the pinned image and the already evidenced layouts in
src/common/halo3_vehicle_logic.h and docs/HALO3-VEHICLE-EVIDENCE.md.
No remote code execution, suspended threads, or process-memory writes.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct

spec = importlib.util.spec_from_file_location("engine_control", Path(__file__).with_name("mcc-engine-control.py"))
engine = importlib.util.module_from_spec(spec)
spec.loader.exec_module(engine)


class ThreadEntry(C.Structure):
    _fields_ = [("size", W.DWORD), ("usage", W.DWORD), ("tid", W.DWORD),
                ("pid", W.DWORD), ("priority", W.LONG), ("delta", W.LONG), ("flags", W.DWORD)]


class ThreadBasic(C.Structure):
    _fields_ = [("exit_status", W.LONG), ("teb", C.c_void_p),
                ("pid", C.c_size_t), ("tid", C.c_size_t), ("affinity", C.c_size_t),
                ("priority", W.LONG), ("base_priority", W.LONG)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kind", type=int, default=0)
    parser.add_argument("--physics", action="store_true", help="Read evidenced object/model/physics tag chain")
    parser.add_argument("--table", type=lambda value: int(value, 0),
                        help="Explicit table from a previous ambiguous-table report")
    args = parser.parse_args()
    proc = engine.only((p for p in engine.enumerate_items(2, 0, engine.Process, "Process32")
                        if p.exe.lower() == "mcc-win64-shipping.exe"), "MCC process")
    module = engine.only((m for m in engine.enumerate_items(0x18, proc.pid, engine.Module, "Module32")
                          if m.name.lower() == "halo3.dll"), "Halo 3 module")
    raw = Path(module.path).read_bytes()
    if hashlib.sha256(raw).hexdigest().upper() != engine.PIN:
        raise ValueError("Halo 3 image differs from the evidence pin")
    pe = engine.pefile.PE(data=raw, fast_load=True)
    pe.parse_data_directories(directories=[9])
    # Windows' TLS directory identifies the module's own index variable.
    tls_rva = pe.DIRECTORY_ENTRY_TLS.struct.AddressOfIndex - pe.OPTIONAL_HEADER.ImageBase
    if tls_rva != 0xA39F9C:
        raise ValueError("TLS directory differs from the evidenced index")
    process = engine.open_process(0x410, False, proc.pid)
    if not process:
        raise C.WinError(C.get_last_error())
    try:
        def read(address, size):
            buf = C.create_string_buffer(size)
            count = C.c_size_t()
            if not engine.read_memory(process, address, buf, size, C.byref(count)) or count.value != size:
                raise C.WinError(C.get_last_error())
            return buf.raw
        def qword(address):
            return struct.unpack("<Q", read(address, 8))[0]
        index = struct.unpack("<I", read(module.base + tls_rva, 4))[0]
        if index > 1087:
            raise ValueError("TLS index is out of range")
        open_thread = engine.api("OpenThread", W.HANDLE, W.DWORD, W.BOOL, W.DWORD)
        query = C.WinDLL("ntdll").NtQueryInformationThread
        query.argtypes = [W.HANDLE, W.ULONG, C.c_void_p, W.ULONG, C.c_void_p]
        query.restype = W.LONG
        tables = set()
        player_units = {}
        # Thread enumeration uses unsuffixed exports (unlike module/process).
        snap = engine.snapshot(4, 0)
        if snap == C.c_void_p(-1).value:
            raise C.WinError(C.get_last_error())
        try:
            first = engine.api("Thread32First", W.BOOL, W.HANDLE, C.POINTER(ThreadEntry))
            next_item = engine.api("Thread32Next", W.BOOL, W.HANDLE, C.POINTER(ThreadEntry))
            entry = ThreadEntry()
            entry.size = C.sizeof(entry)
            ok = first(snap, C.byref(entry))
            while ok:
                if entry.pid == proc.pid:
                    thread = open_thread(0x40, False, entry.tid)
                    if thread:
                        try:
                            basic = ThreadBasic()
                            if query(thread, 0, C.byref(basic), C.sizeof(basic), None) == 0:
                                slots = qword(basic.teb + 0x58)
                                tls = qword(slots + index * 8)
                                table = qword(tls + 0x38)
                                header = read(table, 0x50)
                                maximum, stride = struct.unpack_from("<II", header, 0x20)
                                signature = struct.unpack_from("<I", header, 0x2C)[0]
                                limit = struct.unpack_from("<I", header, 0x3C)[0]
                                if (header[:7] == b"object\0" and maximum == 2048 and stride == 24 and
                                        header[0x29] == 1 and signature == 0x64407440 and limit <= maximum):
                                    tables.add(table)
                                    # Pinned player-unit getter RVA 0xEE48C:
                                    # TLS +0x110, local-user handle array +0xC8.
                                    users = qword(tls + 0x110)
                                    player_units.setdefault(table, set()).add(
                                        struct.unpack("<I", read(users + 0xC8, 4))[0])
                        except OSError:
                            pass  # Thread teardown and uninitialized TLS are expected.
                        finally:
                            engine.close(thread)
                ok = next_item(snap, C.byref(entry))
        finally:
            engine.close(snap)
        if args.table is not None:
            if args.table not in tables:
                raise ValueError("Requested table is not a live validated table")
            tables = {args.table}
        if len(tables) != 1:
            candidates = []
            for address in sorted(tables):
                header = read(address, 0x50)
                candidates.append({"table": hex(address),
                                   "limit": struct.unpack_from("<I", header, 0x3C)[0],
                                   "count": struct.unpack_from("<I", header, 0x40)[0],
                                   "entries": hex(struct.unpack_from("<Q", header, 0x48)[0])})
            raise ValueError(f"Ambiguous live object tables: {candidates}")
        table = engine.only(tables, "live validated object table")
        header = read(table, 0x50)
        limit = struct.unpack_from("<I", header, 0x3C)[0]
        entries = struct.unpack_from("<Q", header, 0x48)[0]
        if limit > 2048:
            raise ValueError("Object table changed during sampling")
        objects = []
        for i in range(limit):
            entry = read(entries + i * 24, 24)
            salt = struct.unpack_from("<H", entry)[0]
            kind = entry[3]
            address = struct.unpack_from("<Q", entry, 0x10)[0]
            if salt < 0x8000 or not address or kind != args.kind:
                continue
            data = read(address, 0x4E0 if kind == 0 else 0x100)
            position = struct.unpack_from("<3f", data, 0x50)
            velocity = struct.unpack_from("<3f", data, 0x74)
            if not all(math.isfinite(v) for v in position + velocity):
                continue
            if read(entries + i * 24, 24) != entry:
                continue
            obj = {"handle": hex(salt << 16 | i), "kind": kind, "data": hex(address),
                   "position": position, "velocity": velocity}
            if kind == 0:
                obj.update(weapon_slot=struct.unpack_from("<b", data, 0x262)[0],
                           weapon_handles=[hex(v) for v in struct.unpack_from("<4I", data, 0x268)],
                           health=struct.unpack_from("<f", data, 0xF4)[0],
                           shield=struct.unpack_from("<f", data, 0xF8)[0],
                           damage_dead=bool(struct.unpack_from("<I", data, 0x110)[0] & 4),
                           character_mode=data[0x4DE])
            if args.physics:
                # Same pinned H3 tag globals and chain as Halo3LoadedTagDefinition
                # and Halo3ContactPhysicsForObject; bounded read-only snapshot.
                try:
                    instances = qword(module.base + 0xA49018)
                    tag_base = qword(module.base + 0x1FCF4C8)
                    def definition(datum):
                        index = datum & 0xFFFF
                        if index == 0xFFFF:
                            raise ValueError("null tag datum")
                        packed = struct.unpack("<I", read(instances + index * 8 + 4, 4))[0]
                        if not packed:
                            raise ValueError("null tag address")
                        return tag_base + packed * 4
                    object_tag = definition(struct.unpack_from("<I", data)[0])
                    model_tag = definition(struct.unpack("<I", read(object_tag + 0x40, 4))[0])
                    physics_tag = definition(struct.unpack("<I", read(model_tag + 0x3C, 4))[0])
                    count, packed = struct.unpack("<iI", read(physics_tag + 0x58, 8))
                    bodies = []
                    if 0 < count <= 32 and packed:
                        for body in range(count):
                            record = read(tag_base + packed * 4 + body * 0xC0, 0x60)
                            shape = struct.unpack_from("<Q", record, 0x58)[0]
                            bodies.append({"node": struct.unpack_from("<h", record)[0],
                                           "shape": hex(shape),
                                           "shape_type": struct.unpack("<i", read(shape + 0x18, 4))[0] if shape else None})
                    obj["physics"] = {"tag": hex(physics_tag), "body_count": count, "bodies": bodies}
                except (OSError, ValueError) as error:
                    obj["physics_error"] = str(error)
            objects.append(obj)
        print(json.dumps({"pid": proc.pid, "table": hex(table),
                          "local_player_units": [hex(v) for v in sorted(player_units.get(table, set()))],
                          "objects": objects}, indent=2))
    finally:
        engine.close(process)


if __name__ == "__main__":
    main()
