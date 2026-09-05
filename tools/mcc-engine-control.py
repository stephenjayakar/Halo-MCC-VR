"""Inspect Halo 3's native command queue, or explicitly enqueue HaloScript.

Read-only unless --script is supplied. No keys, UI automation, game-file edits,
or hardcoded engine address. Requires the pinned Halo 3 image and a live
HALOMCCVR diagnostic session. Use only while that test owns the game session;
do not change titles concurrently. Enqueued is not proof the script succeeded.
Dependencies: pefile (repository out/python-deps), Windows x64 Python.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "out/python-deps"))
import pefile

PIN = "B209D8454B12DC77E54CCD2C9924EC8D44B8619D21CF98E36FFAF601E67EFB63"
k = C.WinDLL("kernel32", use_last_error=True)


def api(name, result, *args):
    fn = getattr(k, name)
    fn.restype, fn.argtypes = result, args
    return fn


snapshot = api("CreateToolhelp32Snapshot", W.HANDLE, W.DWORD, W.DWORD)
close = api("CloseHandle", W.BOOL, W.HANDLE)
open_process = api("OpenProcess", W.HANDLE, W.DWORD, W.BOOL, W.DWORD)
read_memory = api("ReadProcessMemory", W.BOOL, W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t))


class Process(C.Structure):
    _fields_ = [("size", W.DWORD), ("usage", W.DWORD), ("pid", W.DWORD),
                ("heap", C.c_size_t), ("module", W.DWORD), ("threads", W.DWORD),
                ("parent", W.DWORD), ("priority", W.LONG), ("flags", W.DWORD),
                ("exe", W.WCHAR * 260)]


class Module(C.Structure):
    _fields_ = [("size", W.DWORD), ("id", W.DWORD), ("pid", W.DWORD),
                ("global_usage", W.DWORD), ("usage", W.DWORD), ("base", C.c_void_p),
                ("length", W.DWORD), ("handle", W.HANDLE),
                ("name", W.WCHAR * 256), ("path", W.WCHAR * 260)]


def enumerate_items(flags, pid, kind, prefix):
    handle = snapshot(flags, pid)
    if handle == C.c_void_p(-1).value:
        raise C.WinError(C.get_last_error())
    try:
        first = api(prefix + "FirstW", W.BOOL, W.HANDLE, C.POINTER(kind))
        next_item = api(prefix + "NextW", W.BOOL, W.HANDLE, C.POINTER(kind))
        item = kind()
        item.size = C.sizeof(item)
        ok = first(handle, C.byref(item))
        while ok:
            yield kind.from_buffer_copy(item)
            ok = next_item(handle, C.byref(item))
    finally:
        close(handle)


def only(values, label):
    values = list(values)
    if len(values) != 1:
        raise ValueError(f"Expected one {label}; got {len(values)}")
    return values[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--script", help="Explicitly enqueue one HaloScript expression (diagnostic session only)")
    args = parser.parse_args()
    if args.script is not None and (not args.script or len(args.script.encode("ascii")) > 512 or
                                    any(c in args.script for c in "\x00\r\n")):
        raise ValueError("Supply a single ASCII expression of 1..512 bytes")
    proc = only((p for p in enumerate_items(2, 0, Process, "Process32")
                 if p.exe.lower() == "mcc-win64-shipping.exe"), "MCC process")
    modules = list(enumerate_items(0x18, proc.pid, Module, "Module32"))
    title_names = {"halo1.dll", "halo2.dll", "halo2a.dll", "halo3.dll", "halo3odst.dll", "haloreach.dll", "halo4.dll"}
    module = only((m for m in modules if m.name.lower() in title_names), "loaded title")
    if module.name.lower() != "halo3.dll":
        raise ValueError("Only Halo 3 is supported")
    raw = Path(module.path).read_bytes()
    digest = hashlib.sha256(raw).hexdigest().upper()
    if digest != PIN:
        raise ValueError("Halo 3 file identity differs from the evidence pin")
    pe = pefile.PE(data=raw)
    factory = only((e.address for e in pe.DIRECTORY_ENTRY_EXPORT.symbols
                    if e.name == b"CreateGameEngine"), "engine factory export")
    factory_bytes = pe.get_data(factory, 0xD7)
    # The pinned factory allocates 0x460 bytes, installs the vtable, then
    # publishes that same object to its output pointer and module singleton.
    vtable_match = only(re.finditer(rb"\x48\x8D\x05(.{4})\x48\x89\x07", factory_bytes, re.S), "factory vtable publication")
    global_match = only(re.finditer(rb"\x49\x89\x3E\x48\x89\x3D(.{4})", factory_bytes, re.S), "factory singleton publication")
    if bytes.fromhex("BA20000000B960040000") not in factory_bytes:
        raise ValueError("Factory allocation contract changed")
    table_rva = factory + vtable_match.start() + 7 + struct.unpack("<i", vtable_match[1])[0]
    global_rva = factory + global_match.start() + 10 + struct.unpack("<i", global_match[1])[0]
    # Locate the command producer by its complete pinned instruction shape,
    # then verify its reference in the factory's vtable. No copied slot address.
    pattern = re.compile(re.escape(bytes.fromhex("48895C24084889742410574883EC20488BF9488BF24881C140040000FF15")) + rb".{4}" +
                         re.escape(bytes.fromhex("488BD84885C07425834808FF488BCEC6400C01FF15")) + rb".{4}" +
                         re.escape(bytes.fromhex("488D8F30040000488BD348894310FF15")) + rb".{4}" +
                         re.escape(bytes.fromhex("488B5C2430488B7424384883C4205FC3")), re.S)
    match = only(pattern.finditer(raw), "command queue signature")
    command_rva = pe.get_rva_from_offset(match.start())
    table = struct.unpack("<11Q", pe.get_data(table_rva, 88))
    slot = only((i for i, value in enumerate(table) if value == pe.OPTIONAL_HEADER.ImageBase + command_rva), "command vtable slot")
    rights = 0x410 | (0x2A if args.script is not None else 0)
    process = open_process(rights, False, proc.pid)
    if not process:
        raise C.WinError(C.get_last_error())
    allocation = thread = None
    completed = False
    try:
        def read(address, size):
            buf = C.create_string_buffer(size)
            count = C.c_size_t()
            if not read_memory(process, address, buf, size, C.byref(count)) or count.value != size:
                raise C.WinError(C.get_last_error())
            return buf.raw
        def qword(address):
            return struct.unpack("<Q", read(address, 8))[0]
        base = module.base
        engine = qword(base + global_rva)
        if not engine or qword(engine) != base + table_rva:
            raise ValueError("Live engine does not match the factory vtable")
        if qword(base + table_rva + slot * 8) != base + command_rva:
            raise ValueError("Live command slot differs from the pinned producer")
        if read(base + factory, len(factory_bytes)) != factory_bytes or read(base + command_rva, len(match[0])) != match[0]:
            raise ValueError("Live factory or command producer differs from file evidence")
        result = {"pid": proc.pid, "module_sha256": digest, "factory_rva": hex(factory),
                  "singleton_rva": hex(global_rva), "vtable_rva": hex(table_rva),
                  "command_rva": hex(command_rva), "command_slot": slot,
                  "live_engine": hex(engine), "mode": "read-only", "script_executed": False}
        if args.script is not None:
            main_module = only((m for m in modules if m.name.lower() == "mcc-win64-shipping.exe"), "MCC executable")
            install = Path(main_module.path).parents[3]
            log_path = install / "Halo_MCC_VR/halo3xr.log"
            log = log_path.read_text(errors="replace")
            if ("H3 physical contact DEBUG RIG:" not in log or
                    not re.search(r"headset:.*null", log, re.I) or
                    f"loaded into pid {proc.pid} " not in log):
                raise ValueError("Native commands require the null-driver contact diagnostic session")
            alloc = api("VirtualAllocEx", C.c_void_p, W.HANDLE, C.c_void_p, C.c_size_t, W.DWORD, W.DWORD)
            write = api("WriteProcessMemory", W.BOOL, W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t))
            protect = api("VirtualProtectEx", W.BOOL, W.HANDLE, C.c_void_p, C.c_size_t, W.DWORD, C.POINTER(W.DWORD))
            allocation = alloc(process, None, 4096, 0x3000, 4)
            if not allocation:
                raise C.WinError(C.get_last_error())
            payload = ("HS: " + args.script).encode("ascii") + b"\0"
            # Windows x64 shadow space and 16-byte call alignment. Volatile
            # registers only. Producer strdup's the string into its own queue.
            stub = (b"\x48\x83\xEC\x28\x48\xB9" + struct.pack("<Q", engine) +
                    b"\x48\xBA" + struct.pack("<Q", allocation + 256) +
                    b"\x48\xB8" + struct.pack("<Q", base + command_rva) +
                    b"\xFF\xD0\x48\x83\xC4\x28\x33\xC0\xC3")
            block = stub.ljust(256, b"\0") + payload
            count = C.c_size_t()
            if not write(process, allocation, block, len(block), C.byref(count)) or count.value != len(block):
                raise C.WinError(C.get_last_error())
            old = W.DWORD()
            if not protect(process, allocation, 4096, 0x20, C.byref(old)):
                raise C.WinError(C.get_last_error())
            flush = api("FlushInstructionCache", W.BOOL, W.HANDLE, C.c_void_p, C.c_size_t)
            if not flush(process, allocation, len(stub)):
                raise C.WinError(C.get_last_error())
            if qword(base + global_rva) != engine or qword(engine) != base + table_rva:
                raise ValueError("Engine changed before command admission")
            start = api("CreateRemoteThread", W.HANDLE, W.HANDLE, C.c_void_p, C.c_size_t, C.c_void_p, C.c_void_p, W.DWORD, C.POINTER(W.DWORD))
            thread = start(process, None, 0, allocation, None, 0, None)
            if not thread:
                raise C.WinError(C.get_last_error())
            wait = api("WaitForSingleObject", W.DWORD, W.HANDLE, W.DWORD)
            if wait(thread, 5000) != 0:
                raise RuntimeError("Command thread did not complete; allocation retained until process exit")
            completed = True
            code = W.DWORD()
            get_code = api("GetExitCodeThread", W.BOOL, W.HANDLE, C.POINTER(W.DWORD))
            if not get_code(thread, C.byref(code)) or code.value != 0:
                raise RuntimeError(f"Command thread failed: {code.value:#x}")
            result.update(mode="command producer called", script=args.script,
                          note="Queue submission only; verify the requested game effect separately")
        print(json.dumps(result, indent=2))
    finally:
        if thread:
            close(thread)
        if allocation and (not thread or completed):
            free = api("VirtualFreeEx", W.BOOL, W.HANDLE, C.c_void_p, C.c_size_t, W.DWORD)
            free(process, allocation, 0, 0x8000)
        close(process)


if __name__ == "__main__":
    main()
