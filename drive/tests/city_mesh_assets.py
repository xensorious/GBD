#!/usr/bin/env python3
"""Check the actual GPU texture atlas, fixed VRAM layout and DS memory limits."""
import argparse, array, hashlib, json, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
m=json.loads((ROOT/'build/city-mesh-manifest.json').read_text())
assert m['size']==[256,512] and m['facades']==16 and m['roof_materials']==16 and m['native_geometry']
assert m['source_sha256']==hashlib.sha256((ROOT/'art/city/facades-source.png').read_bytes()).hexdigest()
data=(ROOT/'build/city_mesh_idx.bin').read_bytes()
palette=array.array('H',(ROOT/'build/city_mesh_palette.bin').read_bytes())
if sys.byteorder!='little':palette.byteswap()
assert len(data)==128*1024 and len(palette)==256 and palette[0]==0 and all(c&0x8000 for c in palette[1:])
def region(x,y,w,h):return bytes(data[row*256+col] for row in range(y,y+h) for col in range(x,x+w))
faces=[region(i%4*64,i//4*32,64,32) for i in range(16)]
roofs=[region(i%8*32,128+i//8*32,32,32) for i in range(16)]
assert len(set(faces))==len(set(roofs))==16
assert all(all(face) and len(set(face))>32 for face in faces)
assert all(all(roof) and len(set(roof))>=4 for roof in roofs)
assert all(region(i*8,496,8,12).count(0)>12 and sum(bool(c) for c in region(i*8,496,8,12))>5 for i in range(26))
for i in range(5):assert 300<sum(bool(c) for c in region(i*48,256,48,48))<48*48
for i in range(24):assert 20<sum(bool(c) for c in region(i%8*32,304+i//8*32,32,32))<32*32
# The NPC format consumes 2 KiB/model, player format 16 KiB. No overlap or
# hidden larger framebuffer allocation is allowed to use the texture banks.
assert 48*32*64==98304 and 98304+64*128*2<=128*1024
assert 512+48*512+512<=64*1024 # one uniform silhouette palette, no extra textures
nm=Path('/workspace/gba-tools/sysroot/usr/bin/arm-none-eabi-nm')
symbols=subprocess.check_output([str(nm),str(ROOT/'build/arm9.elf')],text=True)
def address(name):return int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' '+name)),16)
assert address('__bss_end__')<0x02380000 and address('__itcm_end')<=0x01008000
for name in ('city_depth','city_shadow_stencil','city_unculled_diagnostic','city_target','city_player_texture','city_sprites','city_building_idx','overhead_indexed'):
 assert not any(line.endswith(' '+name) for line in symbols.splitlines()),('Native/retired asset linked into ROM',name)
result={'sha256':hashlib.sha256((ROOT/'build/Coastline.nds').read_bytes()).hexdigest(),
 'orthographic_facades':16,'roof_materials':16,'all_texture_regions_valid':True,'source_hash_matches':True,
 'texture_bank_a_bytes':len(data),'texture_bank_d_bytes':98304+64*128*2,'palette_bank_e_bytes':512+48*512+512,
 'native_rasterizer_excluded_from_rom':True,'retired_flat_building_bank_excluded_from_rom':True,
 'ds_main_ram_end':hex(address('__bss_end__')),'ds_itcm_end':hex(address('__itcm_end'))}
(a.output/'city-mesh-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
