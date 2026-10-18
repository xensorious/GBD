#!/usr/bin/env python3
"""Finalize the DS-only header produced by a DS/DSi-capable SDK.

The SDK retains optional DSi library sections in the ELF. Coastline targets the
original DS exclusively; its main executable and cartridge backup work in DS
mode. The 0x4000 ARM9 offset and unique game ID are intentional: melonDS uses
them to select standard cartridge EEPROM instead of a homebrew SD adapter.
"""
from pathlib import Path
import struct
import sys

path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
assert data[0x0C:0x10] == b"KCWE", "Unexpected ROM game code"
assert struct.unpack_from("<I", data, 0x20)[0] == 0x4000, "Incorrect ARM9 offset"
data[0x12] = 0  # Nintendo DS, without optional DSi extensions.
crc = 0xFFFF
for value in data[:0x15E]:
    crc ^= value
    for _ in range(8):
        crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
struct.pack_into("<H", data, 0x15E, crc)
path.write_bytes(data)
