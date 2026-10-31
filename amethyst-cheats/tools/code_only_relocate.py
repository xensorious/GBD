"""Bounded relocation of an exact Amethyst callback into reserved RAM.

Reads the user's checksum-matched ROM locally. No ROM is bundled with this tool.
Ordinary game logic stays byte-identical apart from necessary PC/BL fixups and
the documented dispatch pointers. Thumb-1 ARM7TDMI only.
"""
from __future__ import annotations

import hashlib
import struct

SOURCE_SHA256 = '89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430'
ROM_BASE = 0x08000000

# Source code + its final literal pool, ending exactly before the next function.
ACTION_SOURCE, ACTION_END, ACTION_DEST = 0x088CE0CC, 0x088CE8E4, 0x0200E000
CB2_SOURCE, CB2_END, CB2_DEST = 0x088CD010, 0x088CD110, 0x0201BE60
CASE_U16 = 0x08850436


def _u16(data: bytes, offset: int) -> int:
    return struct.unpack_from('<H', data, offset)[0]


def _bl_target(address: int, first: int, second: int) -> int:
    high = (first & 0x7FF) << 12
    if high & 0x400000:
        high -= 0x800000
    return address + 4 + high + ((second & 0x7FF) << 1)


def _bl(address: int, target: int) -> bytes:
    displacement = target - address - 4
    if displacement & 1 or not -0x400000 <= displacement <= 0x3FFFFE:
        raise ValueError('Thumb BL is out of range')
    return struct.pack('<HH', 0xF000 | ((displacement >> 12) & 0x7FF),
                       0xF800 | ((displacement >> 1) & 0x7FF))


def _veneer(target: int) -> bytes:
    # push{r3}; ldr r3,[pc,#8]; mov ip,r3; pop{r3}; bx ip; nop; .word
    # R0-R3, LR and the original SP reach the target unchanged. IP is scratch.
    return bytes.fromhex('08b4024b9c4608bc6047c046') + struct.pack('<I', target | 1)


def _instructions(rom: bytes, start: int, end: int):
    """Find reachable Thumb instructions, BL calls and PC-literal words.

    A control-flow walk avoids interpreting inline switch tables/literal pools
    as instructions. The single GCC u16 switch in ExecuteActions is explicit.
    """
    todo, seen, calls, literals, switch_tables = [start], {}, {}, set(), set()
    while todo:
        address = todo.pop()
        while address not in seen:
            if address & 1 or not start <= address < end:
                raise ValueError(f'Callback branches outside its bounds: {address:08X}')
            first = _u16(rom, address - ROM_BASE)
            width = 2
            if first & 0xF800 == 0xF000:
                if address + 4 > end:
                    raise ValueError('Truncated Thumb BL')
                second = _u16(rom, address - ROM_BASE + 2)
                if second & 0xF800 != 0xF800:
                    raise ValueError('Unsupported Thumb long branch')
                target = _bl_target(address, first, second)
                calls[address] = target
                width = 4
                seen[address] = width
                if target == CASE_U16:
                    if start != ACTION_SOURCE or address != 0x088CE112:
                        raise ValueError('Unexpected inline switch')
                    table = address + 4
                    switch_tables.add((table, table + 10))
                    for index in range(5):
                        displacement = _u16(rom, table - ROM_BASE + index * 2)
                        todo.append(table + displacement * 2)
                    break
                if not ROM_BASE <= target < ROM_BASE + len(rom):
                    raise ValueError(f'Unexpected BL target: {target:08X}')
                address += width
                continue
            seen[address] = width
            if first & 0xF800 == 0x4800:
                literal = ((address + 4) & ~3) + ((first & 0xFF) * 4)
                if not start <= literal <= end - 4:
                    raise ValueError(f'Literal outside callback: {literal:08X}')
                literals.add(literal)
            if first & 0xF800 == 0xA000:
                target = ((address + 4) & ~3) + ((first & 0xFF) * 4)
                if not start <= target < end:
                    raise ValueError('External ADR is unsupported')
            if first & 0xF800 == 0xE000:
                displacement = (first & 0x7FF) << 1
                if displacement & 0x800:
                    displacement -= 0x1000
                todo.append(address + 4 + displacement)
                break
            if first & 0xF000 == 0xD000:
                condition = (first >> 8) & 15
                if condition >= 14:
                    raise ValueError('Undefined instruction/SWI in callback')
                displacement = (first & 0xFF) << 1
                if displacement & 0x100:
                    displacement -= 0x200
                todo.append(address + 4 + displacement)
            if first & 0xFF00 == 0xBD00 or first & 0xFF87 == 0x4700:
                break  # POP PC or BX; ordinary external calls are BL veneers.
            if first & 0xFF87 in (0x4687, 0x4487):
                raise ValueError('Unclassified dynamic PC write in callback')
            address += width
    code_bytes = {byte for address, width in seen.items()
                  for byte in range(address, address + width)}
    for literal in literals:
        if any(byte in code_bytes for byte in range(literal, literal + 4)):
            raise ValueError('Code and literal data overlap')
    for low, high in switch_tables:
        if any(byte in code_bytes for byte in range(low, high)):
            raise ValueError('Code and inline switch data overlap')
    return seen, calls, literals


def _clone(rom: bytes, start: int, end: int, destination: int, limit: int,
           replacements: dict[int, int]) -> bytes:
    instructions, calls, literals = _instructions(rom, start, end)
    output = bytearray(rom[start - ROM_BASE:end - ROM_BASE])
    if len(output) & 3:
        raise ValueError('Callback must end on a word boundary')
    for address, value in replacements.items():
        if address not in literals:
            raise ValueError(f'Replacement is not an actual PC literal: {address:08X}')
        struct.pack_into('<I', output, address - start, value)
    for address in literals - replacements.keys():
        pointer = struct.unpack_from('<I', output, address - start)[0]
        if start <= (pointer & ~1) < end:
            struct.pack_into('<I', output, address - start,
                             destination + (pointer - start))
    veneers = {}
    for address, target in sorted(calls.items()):
        if start <= target < end:
            relocated = destination + target - start
        else:
            if target not in veneers:
                veneers[target] = destination + len(output)
                output += _veneer(target)
            relocated = veneers[target]
        output[address - start:address - start + 4] = _bl(
            destination + address - start, relocated)
    if destination + len(output) > limit:
        raise ValueError('Relocated callback exceeds reserved RAM')
    # Every outgoing BL must land in this clone or one of its veneers.
    for address in calls:
        offset = address - start
        a, b = struct.unpack_from('<HH', output, offset)
        target = _bl_target(destination + offset, a, b)
        if not destination <= target < destination + len(output):
            raise ValueError('Outgoing BL was not relocated')
    return bytes(output)


def clone_parts(rom: bytes, battle_main_guard: int):
    """Return (name, RAM address, bytes) for checked callback clones."""
    if hashlib.sha256(rom).hexdigest() != SOURCE_SHA256:
        raise ValueError('Callback relocation requires the exact Amethyst v1.4.0 ROM')
    if not battle_main_guard & 1 or not 0x02000000 <= battle_main_guard < 0x02040000:
        raise ValueError('battle_main_guard must be a Thumb function in EWRAM')
    # The native CB2 shim exposes the original public callback identity while
    # game logic runs. Preserve the original CD0F0 comparison literal: numerous
    # other engine functions also depend on callback2 == 08011101.
    cb2 = _clone(rom, CB2_SOURCE, CB2_END, CB2_DEST, 0x0201BFA0,
                 {0x088CD0F4: battle_main_guard})
    return [('.cb2clone', CB2_DEST, cb2)]
