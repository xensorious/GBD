#!/usr/bin/env python3
"""Validate original coastal scenery banks, seamless lawn and cartridge memory."""
import argparse,array,hashlib,json,sys,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
manifest=json.loads((ROOT/'build/city-environment-manifest.json').read_text())
assert (manifest['buildings'],manifest['garden_objects'],manifest['wind_poses'])==(16,16,8)
for name,expected in manifest['source_sha256'].items():assert hashlib.sha256((ROOT/'art/city'/name).read_bytes()).hexdigest()==expected
buildings=(ROOT/'build/city_building_idx.bin').read_bytes();gardens=(ROOT/'build/city_garden_idx.bin').read_bytes()
assert len(buildings)==16*64*80 and len(gardens)==24*32*32
assert len({buildings[i*5120:(i+1)*5120] for i in range(16)})==16
for i in range(16):
 data=buildings[i*5120:(i+1)*5120];assert sum(bool(c) for c in data)>1200 and len(set(data)-{0})>24,(i,'empty or overly flat building')
 assert not any(data[:64]) and not any(data[-64:]) and not any(data[y*64] or data[y*64+63] for y in range(80)),(i,'clipped silhouette')
lawn=array.array('H',(ROOT/'build/city_lawn_rgb.bin').read_bytes())
if sys.byteorder!='little':lawn.byteswap()
assert len(lawn)==384*32 and len(set(lawn))>32
for y in range(32):
 row=lawn[y*384:(y+1)*384];assert row[:128]==row[128:256]==row[256:384];assert row[0]==row[127]
assert lawn[:128]==lawn[31*384:31*384+128]
nm=Path('/workspace/gba-tools/sysroot/usr/bin/arm-none-eabi-nm')
symbols=subprocess.check_output([str(nm),str(ROOT/'build/arm9.elf')],text=True)
def address(name):return int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' '+name)),16)
assert address('__bss_end__')<0x02380000 and address('__itcm_end')<=0x01008000
result={'sha256':hashlib.sha256((ROOT/'build/Coastline.nds').read_bytes()).hexdigest(),'buildings':16,
 'garden_objects':16,'wind_poses':8,'sources_match':True,'unique_building_designs':True,'building_silhouettes_unclipped':True,
 'lawn_seams_match':True,'lawn_colors':len(set(lawn)),'ds_main_ram_end':hex(address('__bss_end__')),'ds_itcm_end':hex(address('__itcm_end'))}
(a.output/'city-environment-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
