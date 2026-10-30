"""Build one Delta AR v3 entry for the original, unmodified Amethyst ROM.

The game binary remains private and unchanged. ROM-dependent relocation is
performed locally; no patched game file is produced.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

try:
    from .build_release import EXPECTED, sections
    from .cheat_encoding import _ar3_address, encode_line, ar3_write, ar3_rom_patch16
    from .code_only_relocate import clone_parts
except ImportError:
    from build_release import EXPECTED, sections
    from cheat_encoding import _ar3_address, encode_line, ar3_write, ar3_rom_patch16
    from code_only_relocate import clone_parts

S=0x02010C00
SOURCE_WORD=0x0891CECD
SOURCE_AT=0x080005EC
WALK=0x57414C4B
BATTLE=0x4241544C
RESERVED_DATA=((0x0200b960,0x0200bd60),(0x0200cecc,0x0200ced4),
               (0x0200d100,0x0200eaf0),(0x0200f000,0x0200f200),
               (0x02010038,0x02010070),(0x02010a38,0x02010a48),
               (0x02010b00,0x02010b28),(0x02010c00,0x02010e00))


def equal(address, value, *, skip=1, width=32):
    if skip not in (1,2):raise ValueError('skip must be one or two')
    operation={8:4,16:5,32:6}[width]+(0x20 if skip==2 else 0)
    return encode_line(_ar3_address(operation,address),value)


def gated_patch(slot,address,value,request=None):
    patch=list(ar3_rom_patch16(slot,address,value))
    if request is None:
        return [equal(SOURCE_AT,SOURCE_WORD,skip=2)]+patch
    # Both tests must pass. False branches reach ALWAYS2, skipping the full
    # two-line patch; the true path uses ALWAYS1 to jump past ALWAYS2.
    return [equal(SOURCE_AT,SOURCE_WORD,skip=2),equal(S+108,request),
            encode_line(_ar3_address(7,0),0),
            encode_line(_ar3_address(0x27,0),0)]+patch


def fill(address,data):
    """Halfword FILL avoids DeleteAll restoring payload words to stale data."""
    if address&1 or len(data)&1:raise ValueError('Payload must be halfword aligned')
    if not any(low<=address<=address+len(data)<=high for low,high in RESERVED_DATA):
        raise ValueError('Payload falls outside its allocated intervals')
    return [ar3_write(address+i,int.from_bytes(data[i:i+2],'little'),width=16)
            for i in range(0,len(data),2)]


def build(rom,elf):
    if hashlib.sha256(rom).hexdigest()!=EXPECTED:
        raise ValueError('This code requires the exact supplied Amethyst v1.4.0 ROM')
    parts=sections(elf)
    layout={'.readkeys':(0x0200cecc,8),'.text':(0x0200d100,0xf00),
            '.heapentry':(0x02010a38,16),'.guards':(0x02010b00,20),
            '.callbacks':(0x02010b20,8)}
    if {n for n,a,b in parts}!=set(layout) or len(parts)!=len(layout):
        raise ValueError('Unexpected native sections')
    blobs={}
    for name,address,data in parts:
        expected,capacity=layout[name]
        if address!=expected or not 0<len(data)<=capacity:
            raise ValueError('Unexpected native layout: '+name)
        if name!='.text' and len(data)!=capacity:raise ValueError('Invalid section size')
        blobs[name]=data
    guards=struct.unpack('<5I',blobs['.guards'])
    callback,cb2shim=struct.unpack('<2I',blobs['.callbacks'])
    for pointer in (*guards,callback,cb2shim):
        if not pointer&1 or not 0x0200d100<=pointer<0x0200d100+len(blobs['.text']):
            raise ValueError('Invalid native callback')
    table=bytearray(rom[0x88b960:0x88bd60])
    for op,pointer in zip((0x0b,0x0c,0x19,0x79,0xf4),guards):
        struct.pack_into('<I',table,op*4,pointer)
    payload=[('commands',0x0200b960,bytes(table))]+parts+clone_parts(rom,callback)
    ordered=sorted(payload,key=lambda p:p[1])
    for previous,current in zip(ordered,ordered[1:]):
        if previous[1]+len(previous[2])>current[1]:raise ValueError('Payload overlap')
    lines=[encode_line(0xc4002b80,0),
           encode_line(_ar3_address(0x46,SOURCE_AT),SOURCE_WORD)]
    for name,address,data in payload:lines+=fill(address,data)
    lines+=fill(S+92,struct.pack('<I',0x43484541))
    lines+=[encode_line(0,0x40000000)]
    lines+=gated_patch(0,0x08002b96,0x0201)
    lines+=gated_patch(1,0x08002b84,0x4710)
    lines+=gated_patch(2,0x080005ee,0x0200)
    lines+=gated_patch(3,0x08014c1e,0x0200,BATTLE)
    lines+=gated_patch(3,0x0805bb8c,0x2000,WALK)
    if len(lines)>16384:raise ValueError('VBA-M cheat limit exceeded')
    text='\n'.join(lines) # Delta rejects an extra empty final line.
    manifest={'source_sha256':EXPECTED,'format':'GBA Action Replay v3',
              'requires_rom_patch':False,'activation_lines':len(lines),
              'code_sha256':hashlib.sha256(text.encode()).hexdigest(),
              'runtime_bytes':sum(len(b) for n,a,b in payload),
              'heap_reserved_bytes':sum(high-low+16 for low,high in RESERVED_DATA),
              'master_address':'08002B80',
              'validation':'See CODE_ONLY_VALIDATION.md for tested scope.'}
    return text,manifest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('rom',type=Path)
    p.add_argument('--elf',type=Path,default=Path('build/code_only.elf'))
    p.add_argument('--output',type=Path,default=Path('dist/amethyst-v1.4.0-delta-code.txt'))
    p.add_argument('--manifest',type=Path,default=Path('dist/code-only-manifest.json'))
    args=p.parse_args()
    text,manifest=build(args.rom.read_bytes(),args.elf)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(text)
    args.manifest.write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))

if __name__=='__main__':main()
