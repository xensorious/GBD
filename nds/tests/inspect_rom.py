#!/usr/bin/env python3
"""Check the distributed NDS cartridge header and executable regions."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def crc16(data):
    value = 0xFFFF
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xA001 if value & 1 else 0)
    return value


def inspect(path):
    data = Path(path).read_bytes()
    assert len(data) >= 512, "Truncated NDS header"
    assert crc16(data[:0x15E]) == struct.unpack_from("<H", data, 0x15E)[0], "Bad header checksum"
    assert crc16(data[0xC0:0x15C]) == struct.unpack_from("<H", data, 0x15C)[0], "Bad logo checksum"
    regions = {}
    for name, offset in (("arm9", 0x20), ("arm7", 0x30)):
        file_offset, entry, load, size = struct.unpack_from("<IIII", data, offset)
        assert file_offset >= 512 and size > 0, f"Missing {name} executable"
        assert file_offset + size <= len(data), f"Truncated {name} executable"
        assert load <= (entry & ~1) < load + size, f"Invalid {name} entry point"
        assert len(set(data[file_offset:file_offset + size])) > 8, f"Empty {name} executable"
        regions[name] = {"offset": file_offset, "entry": hex(entry),
                         "load": hex(load), "bytes": size}
    used_size = struct.unpack_from("<I", data, 0x80)[0]
    assert 512 <= used_size <= len(data), "Header used-size exceeds ROM"
    return {"title": data[:12].rstrip(b"\0").decode("ascii"),
            "game_code": data[12:16].decode("ascii"), "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(), "executables": regions}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.rom), indent=2))
