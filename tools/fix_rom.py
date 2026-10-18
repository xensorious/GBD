"""Create a valid original GBA cartridge header; contains no commercial game data."""
from pathlib import Path
import sys

LOGO = bytes.fromhex(
    '24ffae51699aa2213d84820a84e409ad11248b98c0817f21a352be199309ce20'
    '10464a4af82731ec58c7e83382e3cebf85f4df94ce4b09c194568ac01372a7fc'
    '9f844d73a3ca9a615897a327fc039876231dc7610304ae56bf38840040a70efd'
    'ff52fe036f9530f197fbc08560d68025a963be03014e38e2f9a234ffbb3e0344'
    '780090cb88113a9465c07c6387f03cafd625e48b380aac7221d4f807'
)
p = Path(sys.argv[1])
b = bytearray(p.read_bytes())
assert len(LOGO) == 156
b[4:160] = LOGO
b[160:172] = b'BRAMBLEBOUND'
b[172:176] = b'BBNE'
b[176:178] = b'01'
b[178] = 0x96
b[179:189] = bytes(10)
b[189] = (-sum(b[160:189]) - 0x19) & 255
b[190:192] = b'\0\0'
size = max(256 * 1024, 1 << (len(b) - 1).bit_length())
b.extend(b'\xff' * (size - len(b)))
p.write_bytes(b)
print(f'{p}: {len(b):,} bytes; header checksum {b[189]:02X}')
