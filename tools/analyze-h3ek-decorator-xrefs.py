import argparse
import pathlib
import struct
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=pathlib.Path)
    parser.add_argument("needle")
    parser.add_argument("--context", type=int, default=12)
    args = parser.parse_args()

    sys.path.insert(0, str(pathlib.Path(__file__).parents[1] / "out" / "python-deps"))
    import capstone
    import pefile

    pe = pefile.PE(str(args.image), fast_load=True)
    image = args.image.read_bytes()
    needle = args.needle.encode("ascii") + b"\0"
    offsets = []
    start = 0
    while True:
        offset = image.find(needle, start)
        if offset < 0:
            break
        offsets.append(offset)
        start = offset + 1
    if not offsets:
        print("needle not found")
        return 1

    text_section = next(
        section for section in pe.sections
        if section.Name.rstrip(b"\0") == b".text")
    text = text_section.get_data()
    text_va = pe.OPTIONAL_HEADER.ImageBase + text_section.VirtualAddress
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    instructions = list(md.disasm(text, text_va))

    target_vas = []
    for offset in offsets:
        rva = pe.get_rva_from_offset(offset)
        va = pe.OPTIONAL_HEADER.ImageBase + rva
        target_vas.append(va)
        print(f"string file+0x{offset:X} rva=0x{rva:X} va=0x{va:X}")

    reference_targets = set(target_vas)
    frontier = set(target_vas)
    for depth in range(2):
        next_frontier = set()
        for target in frontier:
            encoded = struct.pack("<Q", target)
            start = 0
            while True:
                offset = image.find(encoded, start)
                if offset < 0:
                    break
                try:
                    rva = pe.get_rva_from_offset(offset)
                except pefile.PEFormatError:
                    start = offset + 1
                    continue
                va = pe.OPTIONAL_HEADER.ImageBase + rva
                if va not in reference_targets:
                    next_frontier.add(va)
                print(
                    f"pointer depth={depth + 1} file+0x{offset:X} "
                    f"rva=0x{rva:X} va=0x{va:X} -> 0x{target:X}")
                if depth == 0:
                    dump_start = max(0, offset - 64)
                    dump_end = min(len(image), offset + 128)
                    for row in range(dump_start, dump_end, 32):
                        values = []
                        for cursor in range(row, min(row + 32, dump_end), 8):
                            if cursor + 8 <= len(image):
                                values.append(
                                    f"{struct.unpack_from('<Q', image, cursor)[0]:016X}")
                        print(f"  file+0x{row:X}: {' '.join(values)}")
                start = offset + 1
        reference_targets.update(next_frontier)
        frontier = next_frontier

    hits = []
    for index, instruction in enumerate(instructions):
        for operand in instruction.operands:
            if operand.type != capstone.x86.X86_OP_MEM:
                continue
            mem = operand.mem
            if mem.base != capstone.x86.X86_REG_RIP:
                continue
            referenced = instruction.address + instruction.size + mem.disp
            if referenced in reference_targets:
                hits.append(index)
                break

    for hit in hits:
        print(f"\nxref 0x{instructions[hit].address:X}")
        first = max(0, hit - args.context)
        last = min(len(instructions), hit + args.context + 1)
        for instruction in instructions[first:last]:
            marker = ">" if instruction.address == instructions[hit].address else " "
            print(
                f"{marker} 0x{instruction.address:X}: "
                f"{instruction.mnemonic:<8} {instruction.op_str}")
    print(f"\n{len(hits)} xrefs")
    return 0 if hits else 2


if __name__ == "__main__":
    raise SystemExit(main())
