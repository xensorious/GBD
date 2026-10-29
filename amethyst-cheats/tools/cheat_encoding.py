"""Encode the encrypted GBA cheat formats used by Delta's VBA-M core.

This module encodes operations; it does not establish that an address is safe
or that an operation is a working game cheat. AR v3 is the current Delta path
for BOTH the Action Replay and GameShark selectors. The v1 helpers are retained
for analysis and other emulators, not as a supported Delta installation path.

Semantics: VBA-M Cheats.cpp at 453fa0decf179360926fb417725a794aa496d6e3.
Default (DEADFACE = 0) seeds only: do not mix with codes changing DEADFACE.
"""

from collections.abc import Iterable


_MASK = 0xFFFFFFFF
_DELTA = 0x9E3779B9
_KEYS = {
    1: (0x09F4FBBD, 0x9681884A, 0x352027E9, 0xF3DEE5A7),
    3: (0x7AA9648F, 0x7FAE6994, 0xC0EFAAD5, 0x42712C57),
}


def _unsigned(value: int, bits: int, label: str) -> int:
    if type(value) is not int or not 0 <= value < 1 << bits:
        raise ValueError(f"{label} must be an unsigned {bits}-bit integer")
    return value


def tea_encrypt(address: int, value: int, *, version: int = 3) -> tuple[int, int]:
    """Encrypt one raw pair with VBA-M's default GSA v1 or AR v3 key."""
    a = _unsigned(address, 32, "address")
    b = _unsigned(value, 32, "value")
    if version not in _KEYS:
        raise ValueError("version must be 1 or 3")
    key = _KEYS[version]
    rolling = 0
    for _ in range(32):
        rolling = (rolling + _DELTA) & _MASK
        a = (a + (((b << 4) + key[0]) ^ (b + rolling) ^ ((b >> 5) + key[1]))) & _MASK
        b = (b + (((a << 4) + key[2]) ^ (a + rolling) ^ ((a >> 5) + key[3]))) & _MASK
    return a, b


def tea_decrypt(address: int, value: int, *, version: int = 3) -> tuple[int, int]:
    """Decrypt one encrypted pair as cheatsDecryptGSACode does."""
    a = _unsigned(address, 32, "address")
    b = _unsigned(value, 32, "value")
    if version not in _KEYS:
        raise ValueError("version must be 1 or 3")
    key = _KEYS[version]
    rolling = 0xC6EF3720
    for _ in range(32):
        b = (b - (((a << 4) + key[2]) ^ (a + rolling) ^ ((a >> 5) + key[3]))) & _MASK
        a = (a - (((b << 4) + key[0]) ^ (b + rolling) ^ ((b >> 5) + key[1]))) & _MASK
        rolling = (rolling - _DELTA) & _MASK
    return a, b


def encode_line(address: int, value: int, *, version: int = 3) -> str:
    a, b = tea_encrypt(address, value, version=version)
    return f"{a:08X} {b:08X}"


def decode_line(code: str, *, version: int = 3) -> tuple[int, int]:
    text = code.replace(" ", "")
    if len(text) != 16 or any(c not in "0123456789abcdefABCDEF" for c in text):
        raise ValueError("a code line must contain exactly sixteen hexadecimal digits")
    return tea_decrypt(int(text[:8], 16), int(text[8:], 16), version=version)


def _ram_address(address: int, width: int) -> int:
    _unsigned(address, 32, "address")
    if width not in (8, 16, 32):
        raise ValueError("width must be 8, 16, or 32")
    size = width // 8
    if address % size:
        raise ValueError("address must be naturally aligned")
    if not (0x02000000 <= address <= 0x02040000 - size
            or 0x03000000 <= address <= 0x03008000 - size):
        raise ValueError("address must be within GBA EWRAM or IWRAM")
    return address


def _ar3_address(operation: int, address: int) -> int:
    # Inverse of the type/address extraction in cheatsAddGSACode(v3=true).
    return ((operation & 0x7F) << 25 | (operation & 0x80) << 17
            | (address & 0x0F000000) >> 4 | address & 0x0003FFFF)


def ar3_write(address: int, value: int, *, width: int = 32) -> str:
    """One constant RAM write. ROM patching requires ar3_rom_patch16."""
    _ram_address(address, width)
    _unsigned(value, width, "value")
    operation = {8: 0x00, 16: 0x01, 32: 0x02}[width]
    return encode_line(_ar3_address(operation, address), value)


def ar3_write_block(address: int, data: bytes) -> list[str]:
    """Write exact bytes using aligned words followed by a halfword/byte tail."""
    if not isinstance(data, bytes):
        raise ValueError("data must be bytes")
    if not data:
        return []
    _ram_address(address, 8)
    _ram_address(address + len(data) - 1, 8)
    lines = []
    offset = 0
    while offset < len(data):
        current = address + offset
        remaining = len(data) - offset
        width = 32 if current % 4 == 0 and remaining >= 4 else (
            16 if current % 2 == 0 and remaining >= 2 else 8)
        size = width // 8
        value = int.from_bytes(data[offset:offset + size], "little")
        lines.append(ar3_write(current, value, width=width))
        offset += size
    return lines


def ar3_if_equal(address: int, value: int, *, width: int = 16,
                 skip: int = 1) -> str:
    """Skip the next one or two code lines when RAM does not equal value.

    Use skip=2 to gate a two-line ROM patch operation. Global on/off conditions
    do not gate the separate ROM-patch switch in this VBA-M revision.
    """
    _ram_address(address, width)
    _unsigned(value, width, "value")
    if skip not in (1, 2):
        raise ValueError("skip must be one or two lines")
    operation = {8: 0x04, 16: 0x05, 32: 0x06}[width] + (0x20 if skip == 2 else 0)
    return encode_line(_ar3_address(operation, address), value)


def ar3_rom_patch16(slot: int, address: int, value: int) -> tuple[str, str]:
    """Use one of VBA-M's four reversible AR v3 halfword ROM patch slots.

    Each slot may target only one halfword per cheat pass. Passing more than
    four distinct slots is impossible; repeats overwrite the earlier slot.
    """
    if type(slot) is not int or slot not in range(4):
        raise ValueError("slot must be an integer from zero through three")
    _unsigned(value, 16, "value")
    if type(address) is not int or address & 1 or not 0x08000000 <= address <= 0x09FFFFFE:
        raise ValueError("ROM address must be an aligned halfword within a 32 MiB cartridge")
    offset = (address - 0x08000000) >> 1
    return (encode_line(0, (0x0C + slot) << 25 | offset),
            encode_line(value, 0))


def gsa1_write(address: int, value: int, *, width: int = 32) -> str:
    """Research/other-emulator helper: current Delta does not select GSA v1."""
    _ram_address(address, width)
    _unsigned(value, width, "value")
    operation = {8: 0, 16: 1, 32: 2}[width]
    return encode_line(operation << 28 | address, value, version=1)


def gsa1_rom_patch16(address: int, value: int) -> str:
    """Research-only v1 per-entry ROM patch, unavailable via current Delta UI."""
    _unsigned(value, 16, "value")
    if type(address) is not int or address & 1 or not 0x08000000 <= address <= 0x09FFFFFE:
        raise ValueError("ROM address must be an aligned halfword within a 32 MiB cartridge")
    return encode_line(0x60000000 | address >> 1, value, version=1)


def format_codes(lines: Iterable[str]) -> str:
    """Join encrypted lines for a Delta cheat field, with a final newline."""
    values = list(lines)
    for line in values:
        decode_line(line)
    return "\n".join(values) + ("\n" if values else "")
