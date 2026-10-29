"""Create a checksum-checked BPS patch and a short Delta activation code.

Only original code and edits are distributed; the user's ROM stays local.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib
try:  # Support both python -m tools.build_release and direct script use.
    from .cheat_encoding import _ar3_address, encode_line, ar3_write, format_codes
except ImportError:
    from cheat_encoding import _ar3_address, encode_line, ar3_write, format_codes

EXPECTED = '89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430'

def sections(path):
    """Read the complete allocated payload from an ARM ELF32 image."""
    elf = Path(path).read_bytes()
    if len(elf) < 52 or elf[:6] != b'\x7fELF\x01\x01':
        raise ValueError('Expected ELF32 little-endian')
    if struct.unpack_from('<H', elf, 18)[0] != 40:
        raise ValueError('Expected an ARM ELF')
    offset = struct.unpack_from('<I', elf, 32)[0]
    size, count, names_index = struct.unpack_from('<HHH', elf, 46)
    if size != 40 or not count or names_index >= count or offset + count * size > len(elf):
        raise ValueError('Invalid ELF section headers')
    headers = [struct.unpack_from('<10I', elf, offset + i * size) for i in range(count)]
    names = headers[names_index]
    if names[1] != 3 or names[4] + names[5] > len(elf):
        raise ValueError('Invalid ELF string table')
    strings = elf[names[4]:names[4] + names[5]]
    parts = []
    for h in headers:
        if not h[2] & 2 or not h[5]:
            continue
        if h[1] != 1 or h[4] + h[5] > len(elf):
            raise ValueError('Allocated ELF section is not complete ROM data')
        if h[0] >= len(strings) or b'\0' not in strings[h[0]:]:
            raise ValueError('Invalid ELF section name')
        name = strings[h[0]:].split(b'\0', 1)[0].decode('ascii')
        parts.append((name, h[3], elf[h[4]:h[4] + h[5]]))
    return parts


def validate_payload(parts):
    """Reject stale or mislinked images before generating a usable patch."""
    layout = {'.text': (0x09ff0000, 0x1000),
              '.readkeys': (0x09ff1000, 8),
              '.heapentry': (0x09ff1010, 8),
              '.collision': (0x09ff1020, 0x40),
              '.guards': (0x09ff1060, 20)}
    names = [name for name, _, _ in parts]
    if len(names) != len(layout) or set(names) != set(layout):
        raise ValueError('Unexpected or missing runtime sections')
    data_by_name = {}
    for name, address, data in parts:
        expected, capacity = layout[name]
        if address != expected or not 0 < len(data) <= capacity:
            raise ValueError(f'Invalid runtime section layout: {name}')
        if name in ('.readkeys', '.heapentry', '.guards') and len(data) != capacity:
            raise ValueError(f'Invalid runtime section size: {name}')
        data_by_name[name] = data
    pointers = struct.unpack('<5I', data_by_name['.guards'])
    for pointer in pointers:
        if not pointer & 1 or not 0x09ff0000 <= (pointer & ~1) < 0x09ff0000 + len(data_by_name['.text']):
            raise ValueError('Battle guard is not a Thumb function in the runtime')
    return pointers


def number(value):
    if not isinstance(value, int) or value < 0:
        raise ValueError("BPS numbers must be nonnegative integers")
    out=bytearray()
    while True:
        byte=value&127;value>>=7
        if not value:out.append(byte|128);return bytes(out)
        out.append(byte);value-=1

def make_bps(source,target):
    if len(source) != len(target):
        raise ValueError('This patch builder requires equal source and target sizes')
    metadata=b'Amethyst v1.4.0 button cheats; apply to the exact source checksum.'
    patch=bytearray(b'BPS1'+number(len(source))+number(len(target))+number(len(metadata))+metadata)
    position=0
    while position<len(target):
        equal=source[position]==target[position]
        end=position+1
        while end<len(target) and (source[end]==target[end])==equal:end+=1
        patch+=number(((end-position-1)<<2)|(0 if equal else 1))
        if not equal:patch+=target[position:end]
        position=end
    patch+=struct.pack('<II',zlib.crc32(source),zlib.crc32(target))
    patch+=struct.pack('<I',zlib.crc32(patch))
    return bytes(patch)

def apply_bps(source, patch):
    """Verify and apply the two BPS actions emitted by this builder."""
    if len(patch) < 19 or patch[:4] != b'BPS1':
        raise ValueError('Not a complete BPS patch')
    expected_source, expected_target, expected_patch = struct.unpack('<III', patch[-12:])
    if zlib.crc32(patch[:-4]) != expected_patch or zlib.crc32(source) != expected_source:
        raise ValueError('BPS checksum mismatch')
    cursor, actions_end = 4, len(patch) - 12

    def read_number():
        nonlocal cursor
        value, shift = 0, 1
        while cursor < actions_end:
            byte = patch[cursor]
            cursor += 1
            value += (byte & 127) * shift
            if byte & 128:
                return value
            shift <<= 7
            value += shift
        raise ValueError('Truncated BPS number')

    source_size, target_size, meta_size = read_number(), read_number(), read_number()
    if source_size != len(source):
        raise ValueError('Wrong source length')
    cursor += meta_size
    if cursor > actions_end:
        raise ValueError('Truncated BPS metadata')
    target = bytearray()
    while len(target) < target_size:
        action = read_number()
        length = (action >> 2) + 1
        if length > target_size - len(target):
            raise ValueError('BPS action exceeds target length')
        if action & 3 == 0:
            if len(target) + length > len(source):
                raise ValueError('BPS source read exceeds source length')
            target += source[len(target):len(target) + length]
        elif action & 3 == 1:
            if cursor + length > actions_end:
                raise ValueError('Truncated BPS target data')
            target += patch[cursor:cursor + length]
            cursor += length
        else:
            raise ValueError('Unexpected copy action')
    if cursor != actions_end or zlib.crc32(target) != expected_target:
        raise ValueError('BPS target or trailing data mismatch')
    return bytes(target)


def activation_code():
    """Gate the sole heartbeat write on the patched hook and reserved state."""
    return format_codes([
        encode_line(_ar3_address(0x46, 0x080005ec), 0x09ff1001),
        encode_line(_ar3_address(0x46, 0x0201be00), 0x414d5954),
        ar3_write(0x0201be5c, 0x43484541),
        encode_line(0, 0x40000000),
    ])


def build(rom,elf):
    source=Path(rom).read_bytes()
    if hashlib.sha256(source).hexdigest()!=EXPECTED:raise ValueError('Wrong Amethyst build; refusing to patch')
    target=bytearray(source)
    parts=sections(elf)
    pointers=validate_payload(parts)
    for name,address,data in parts:
        offset=address-0x08000000
        if offset<0x1ff0000 or offset+len(data)>len(source):raise ValueError('Payload outside reserved ROM space')
        if source[offset:offset+len(data)]!=b'\xff'*len(data):raise ValueError('ROM space is occupied')
        target[offset:offset+len(data)]=data
    changes={
        0x080005ec:(struct.pack('<I',0x0891cecd),struct.pack('<I',0x09ff1001)),
        0x08002b80:(bytes.fromhex('00b5044a1060044a'),bytes.fromhex('004a1047')+struct.pack('<I',0x09ff1011)),
        0x0805bb1c:(bytes.fromhex('70b582b00006060e'),bytes.fromhex('004b1847')+struct.pack('<I',0x09ff1021)),
    }
    originals=[0x088fd309,0x088fd4e5,0x088fe135,0x080271c1,0x0802dfbd]
    for opcode,old,new in zip([0x0b,0x0c,0x19,0x79,0xf4],originals,pointers):
        changes[0x0888b960+opcode*4]=(struct.pack('<I',old),struct.pack('<I',new))
    for address,(old,new) in changes.items():
        offset=address-0x08000000
        if source[offset:offset+len(old)]!=old:raise ValueError(f'Unexpected code at {address:08X}')
        target[offset:offset+len(new)]=new
    target=bytes(target)
    patch=make_bps(source,target)
    if apply_bps(source,patch)!=target:raise AssertionError('Patch roundtrip failed')
    return target,patch,activation_code()

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('rom',type=Path)
    parser.add_argument('--elf',type=Path,default=Path('build/runtime.elf'))
    parser.add_argument('--out',type=Path,default=Path('dist'))
    parser.add_argument('--patched-copy',type=Path)
    args=parser.parse_args()
    target,patch,code=build(args.rom,args.elf)
    args.out.mkdir(parents=True,exist_ok=True)
    (args.out/'amethyst-v1.4.0-button-cheats.bps').write_bytes(patch)
    (args.out/'delta-action-replay.txt').write_text(code)
    manifest={'source_sha256':EXPECTED,'patched_sha256':hashlib.sha256(target).hexdigest(),
              'bps_bytes':len(patch),'activation_lines':len(code.splitlines()),'format':'GBA Action Replay v3',
              'validation':'Development build; see VALIDATION.md for tested scope.'}
    (args.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if args.patched_copy:args.patched_copy.write_bytes(target)
    print(json.dumps(manifest,indent=2));print(code)
