"""Read-only Halo 3 loaded-tag probe for decorator layout evidence.

The probe attaches to an already running MCC process, resolves the two loaded-
tag globals from the same unique retail signature used by the DLL, and prints
bounded candidate BSP/cluster/decorator records.  It never writes process
memory.  Results are diagnostic evidence only; no candidate binding should be
shipped from a heuristic result.
"""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
import math
import pathlib
import struct
import time


TH32CS_SNAPPROCESS = 0x00000002
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("th32DefaultHeapID", ctypes.c_size_t),
        ("th32ModuleID", wintypes.DWORD),
        ("cntThreads", wintypes.DWORD),
        ("th32ParentProcessID", wintypes.DWORD),
        ("pcPriClassBase", wintypes.LONG),
        ("dwFlags", wintypes.DWORD),
        ("szExeFile", wintypes.WCHAR * 260),
    ]


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("th32ModuleID", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("GlblcntUsage", wintypes.DWORD),
        ("ProccntUsage", wintypes.DWORD),
        ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
        ("modBaseSize", wintypes.DWORD),
        ("hModule", wintypes.HMODULE),
        ("szModule", wintypes.WCHAR * 256),
        ("szExePath", wintypes.WCHAR * 260),
    ]


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.ReadProcessMemory.restype = wintypes.BOOL


def close(handle: int) -> None:
    if handle and handle != INVALID_HANDLE_VALUE:
        kernel32.CloseHandle(handle)


def find_process(name: str) -> int | None:
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == INVALID_HANDLE_VALUE:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        ok = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
        while ok:
            if entry.szExeFile.casefold() == name.casefold():
                return int(entry.th32ProcessID)
            ok = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        close(snapshot)
    return None


def find_module(pid: int, name: str) -> tuple[int, int] | None:
    snapshot = kernel32.CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snapshot == INVALID_HANDLE_VALUE:
        return None
    try:
        entry = MODULEENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        ok = kernel32.Module32FirstW(snapshot, ctypes.byref(entry))
        while ok:
            if entry.szModule.casefold() == name.casefold():
                return ctypes.addressof(entry.modBaseAddr.contents), int(entry.modBaseSize)
            ok = kernel32.Module32NextW(snapshot, ctypes.byref(entry))
    finally:
        close(snapshot)
    return None


class Reader:
    def __init__(self, pid: int):
        self.handle = kernel32.OpenProcess(
            PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())

    def close(self) -> None:
        close(self.handle)
        self.handle = None

    def read(self, address: int, size: int) -> bytes:
        buffer = (ctypes.c_ubyte * size)()
        got = ctypes.c_size_t()
        if not kernel32.ReadProcessMemory(
            self.handle, ctypes.c_void_p(address), buffer, size,
            ctypes.byref(got)) or got.value != size:
            raise OSError(f"ReadProcessMemory failed at 0x{address:X} size 0x{size:X}")
        return bytes(buffer)

    def u64(self, address: int) -> int:
        return struct.unpack("<Q", self.read(address, 8))[0]


def parse_pattern(text: str) -> tuple[bytes, bytes]:
    values = bytearray()
    mask = bytearray()
    for token in text.split():
        if token == "??":
            values.append(0)
            mask.append(0)
        else:
            values.append(int(token, 16))
            mask.append(0xFF)
    return bytes(values), bytes(mask)


def find_pattern(data: bytes, pattern: bytes, mask: bytes) -> list[int]:
    anchor = next(i for i, value in enumerate(mask) if value)
    hits = []
    start = 0
    while True:
        pos = data.find(pattern[anchor:anchor + 1], start)
        if pos < 0:
            break
        candidate = pos - anchor
        if candidate >= 0 and candidate + len(pattern) <= len(data) and all(
            not mask[i] or data[candidate + i] == pattern[i]
            for i in range(len(pattern))
        ):
            hits.append(candidate)
        start = pos + 1
    return hits


def fourcc(value: int) -> str | None:
    raw = struct.pack("<I", value)
    if all(32 <= value < 127 for value in raw):
        return raw.decode("ascii")
    return None


def finite_bounds(data: bytes, offset: int) -> bool:
    values = struct.unpack_from("<6f", data, offset)
    for low, high in zip(values[0::2], values[1::2]):
        sentinel = low > 3.0e38 and high > 3.0e38
        if sentinel:
            continue
        if not math.isfinite(low) or not math.isfinite(high):
            return False
        if abs(low) > 100000.0 or abs(high) > 100000.0 or low > high:
            return False
    return True


def decorator_block_score(reader: Reader, tag_base: int, elements: bytes,
                          count: int, stride: int, offset: int) -> tuple[int, list[dict]]:
    nonempty = 0
    groups = []
    for index in range(count):
        element = index * stride + offset
        group_count, address = struct.unpack_from("<II", elements, element)
        if group_count > 128 or (group_count and not address):
            return -1, []
        if not group_count:
            continue
        try:
            raw = reader.read(tag_base + address * 4, group_count * 0x3C)
        except OSError:
            return -1, []
        for group_index in range(group_count):
            base = group_index * 0x3C
            placements, set_index, buffer_index, buffer_offset = struct.unpack_from(
                "<HBBI", raw, base)
            values = struct.unpack_from("<10f", raw, base + 8)
            model_count, model_address = struct.unpack_from("<II", raw, base + 0x30)
            if (not placements or placements > 65535 or set_index >= 64 or
                    buffer_index >= 64 or not all(math.isfinite(v) for v in values) or
                    model_count > 256 or (model_count and not model_address)):
                return -1, []
            groups.append({
                "cluster": index,
                "placement_count": placements,
                "set_index": set_index,
                "buffer_index": buffer_index,
                "buffer_offset": buffer_offset,
                "model_count": model_count,
            })
            nonempty += 1
    return nonempty, groups


def inspect_sbsp(reader: Reader, tag_base: int, datum: int, root: int) -> dict:
    result: dict = {"datum": f"0x{datum:08X}", "root": f"0x{root:X}", "cluster_candidates": []}
    header = reader.read(root, 0x800)
    for offset in range(0, len(header) - 8, 4):
        count, address = struct.unpack_from("<II", header, offset)
        if count < 10 or count > 128 or not address:
            continue
        for stride in range(0x60, 0x181, 4):
            try:
                elements = reader.read(tag_base + address * 4, count * stride)
            except OSError:
                continue
            valid_bounds = sum(finite_bounds(elements, i * stride) for i in range(count))
            if valid_bounds < 24:
                continue
            decorator_options = []
            for field_offset in range(0x30, stride - 7, 4):
                score, groups = decorator_block_score(
                    reader, tag_base, elements, count, stride, field_offset)
                if score > 0:
                    decorator_options.append({
                        "offset": f"0x{field_offset:X}",
                        "nonempty_groups": score,
                        "placements": sum(g["placement_count"] for g in groups),
                        "groups": groups[:64],
                    })
            result["cluster_candidates"].append({
                "root_offset": f"0x{offset:X}",
                "cluster_count": count,
                "address_dword": f"0x{address:X}",
                "stride": f"0x{stride:X}",
                "valid_bounds": valid_bounds,
                "decorator_options": decorator_options,
            })
    return result


def inspect_valhalla_scenario_decorators(
        reader: Reader, tag_base: int, datum: int, root: int) -> list[dict]:
    """Find only the exact H3EK-authored Valhalla scenario decorator block.

    H3EK's scenario_decorator_block is 0x84 bytes. Its decorator count is at
    +0x34, palette tag_block at +0x6C, and decorator-set tag_block at +0x78.
    Each decorator_scenario_set_block element is 0x10 bytes: a four-byte tag
    reference followed by a twelve-byte placements tag_block. Placements are
    exact 0x18-byte global_decorator_placement records. These constants come
    from the official H3EK reflection definitions and printed Riverworld tag,
    not from a retail layout guess.
    """
    results = []
    try:
        header = reader.read(root, 0x800)
    except OSError:
        return results
    for offset in range(0, len(header) - 12, 4):
        count, address = struct.unpack_from("<II", header, offset)
        if count != 1 or not address:
            continue
        try:
            block = reader.read(tag_base + address * 4, 0x84)
        except OSError:
            continue
        decorator_count, current_bsp_count = struct.unpack_from("<II", block, 0x34)
        palette_count, palette_address = struct.unpack_from("<II", block, 0x6C)
        set_count, set_address = struct.unpack_from("<II", block, 0x78)
        if (decorator_count != 25971 or current_bsp_count != 25963 or
                palette_count != 3 or not palette_address or
                set_count != 11 or not set_address):
            continue
        vectors = struct.unpack_from("<12f", block, 0x3C)
        if not all(math.isfinite(value) for value in vectors):
            continue
        result = {
            "datum": f"0x{datum:08X}",
            "root": f"0x{root:X}",
            "root_block_offset": f"0x{offset:X}",
            "decorator_count": decorator_count,
            "current_bsp_count": current_bsp_count,
            "palette_count": palette_count,
            "set_count": set_count,
            "placement_total": 0,
            "placements_valid": False,
            "sets": [],
        }
        try:
            sets = reader.read(tag_base + set_address * 4, set_count * 0x10)
        except OSError:
            result["failure"] = "decorator set block is not readable"
            results.append(result)
            continue
        placement_sets = []
        total_placements = 0
        valid = True
        for set_index in range(set_count):
            element = set_index * 0x10
            decorator_set, placement_count, placement_address = struct.unpack_from(
                "<III", sets, element)
            if (decorator_set == 0xFFFFFFFF or not placement_count or
                    placement_count > decorator_count or not placement_address):
                valid = False
                break
            try:
                samples = reader.read(
                    tag_base + placement_address * 4,
                    placement_count * 0x18)
            except OSError:
                valid = False
                break
            for placement_index in {0, placement_count - 1}:
                position = struct.unpack_from("<3f", samples, placement_index * 0x18)
                if (not all(math.isfinite(value) for value in position) or
                        any(abs(value) > 10000.0 for value in position)):
                    valid = False
                    break
            if not valid:
                break
            total_placements += placement_count
            placement_sets.append({
                "set_index": set_index,
                "decorator_set_datum": f"0x{decorator_set:08X}",
                "placement_count": placement_count,
                "placement_address_dword": f"0x{placement_address:08X}",
            })
        result["placement_total"] = total_placements
        result["sets"] = placement_sets
        result["placements_valid"] = (
            valid and total_placements in (decorator_count, current_bsp_count))
        if not valid:
            result["failure"] = "a placement block failed H3EK bounds checks"
        elif not result["placements_valid"]:
            result["failure"] = "placement total did not match either authored count"
        results.append(result)
    return results


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--wait-seconds", type=int, default=120)
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--scenario-only", action="store_true")
    args = parser.parse_args()
    deadline = time.monotonic() + args.wait_seconds
    pid = None
    module = None
    while time.monotonic() < deadline:
        pid = find_process("MCC-Win64-Shipping.exe")
        module = find_module(pid, "halo3.dll") if pid else None
        if pid and module:
            break
        time.sleep(1)
    if not pid or not module:
        raise SystemExit("Halo 3 module was not loaded before the timeout")

    reader = Reader(pid)
    try:
        module_base, module_size = module
        image = reader.read(module_base, module_size)
        signature = (
            "48 8B 05 ?? ?? ?? ?? 0F B7 C9 8B 54 C8 04 85 D2 75 04 33 D2 EB 0B "
            "48 8B 05 ?? ?? ?? ?? 48 8D 14 90 33 C0 45 33 C0 "
            "48 81 C2 C0 02 00 00 B9 0B 00 00 00 83 3A 00"
        )
        pattern, mask = parse_pattern(signature)
        hits = find_pattern(image, pattern, mask)
        if len(hits) != 1:
            raise RuntimeError(f"vehicle type signature had {len(hits)} matches")
        hit = module_base + hits[0]
        instance_slot = hit + 7 + struct.unpack_from("<i", image, hits[0] + 3)[0]
        tag_base_slot = hit + 29 + struct.unpack_from("<i", image, hits[0] + 25)[0]
        instances = 0
        tag_base = 0
        while time.monotonic() < deadline:
            instances = reader.u64(instance_slot)
            tag_base = reader.u64(tag_base_slot)
            if instances and tag_base:
                break
            time.sleep(1)
        if not instances or not tag_base:
            raise RuntimeError("loaded-tag globals stayed null before the timeout")
        entries = reader.read(instances, 0x10000 * 8)
        group_definitions = []
        sbsp_entries = []
        visited_roots = set()
        readable_roots = 0
        roots_with_candidate_block_counts = 0
        valhalla_scenario_decorators = []
        for index in range(0x10000):
            group, address = struct.unpack_from("<II", entries, index * 8)
            name = fourcc(group)
            if name and address:
                if name in ("psbs", "rncs"):
                    group_definitions.append({
                        "index": index,
                        "group": name,
                        "address_dword": f"0x{address:08X}",
                    })
                # Blam group tags are stored as big-endian four-character
                # integers, so little-endian process bytes spell sbsp as psbs.
                if name == "psbs" and address != 0xFFFFFFFF:
                    root = tag_base + address * 4
                    sbsp_entries.append(inspect_sbsp(reader, tag_base, index, root))
        # The instance table's first dword is a name/string identifier, not a
        # reliable per-instance group tag. Search every bounded readable root
        # for the official Valhalla cluster count and validate the cluster
        # bounds/stride before treating it as a BSP candidate.
        for index in range(0x10000):
            _, address = struct.unpack_from("<II", entries, index * 8)
            # Retail's tag base is a reserved low sentinel. Valid packed
            # dword addresses commonly exceed 0x40000000 before the proven
            # `tag_base + address * 4` conversion reaches committed memory.
            # The engine accessor applies no high-bit cutoff either.
            if not address or address == 0xFFFFFFFF:
                continue
            root = tag_base + address * 4
            if root in visited_roots:
                continue
            visited_roots.add(root)
            try:
                header = reader.read(root, 0x800)
            except OSError:
                continue
            readable_roots += 1
            if not any(
                10 <= struct.unpack_from("<I", header, offset)[0] <= 128
                for offset in range(0, len(header) - 8, 4)
            ):
                continue
            roots_with_candidate_block_counts += 1
            if not args.scenario_only:
                candidate = inspect_sbsp(reader, tag_base, index, root)
                if candidate["cluster_candidates"]:
                    sbsp_entries.append(candidate)
            valhalla_scenario_decorators.extend(
                inspect_valhalla_scenario_decorators(
                    reader, tag_base, index, root))
        report = {
            "pid": pid,
            "module_base": f"0x{module_base:X}",
            "module_size": f"0x{module_size:X}",
            "signature_rva": f"0x{hits[0]:X}",
            "instance_slot": f"0x{instance_slot:X}",
            "tag_base_slot": f"0x{tag_base_slot:X}",
            "instances": f"0x{instances:X}",
            "tag_base": f"0x{tag_base:X}",
            "visited_root_addresses": len(visited_roots),
            "readable_root_addresses": readable_roots,
            "roots_with_candidate_block_counts": roots_with_candidate_block_counts,
            "group_definition_entries": group_definitions,
            "sbsp": sbsp_entries,
            "valhalla_scenario_decorators": valhalla_scenario_decorators,
        }
        text = json.dumps(report, indent=2)
        print(text)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(text + "\n", encoding="utf-8")
    finally:
        reader.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
