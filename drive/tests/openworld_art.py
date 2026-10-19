#!/usr/bin/env python3
"""Independent reviewed plan-view landmarks and cartridge memory budget."""
import argparse,array,hashlib,json,sys,subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
art=ROOT/'art/overhead';source=Image.open(art/'vehicles.png').convert('RGBA');mask=Image.open(art/'materials.png')
assert source.size==mask.size==(384,432) and mask.mode=='P'
manifest=json.loads((art/'materials.json').read_text());assert len(manifest)==48
palette=array.array('H',(ROOT/'build/overhead_palette.bin').read_bytes())
if sys.byteorder!='little':palette.byteswap()
counts=[]
for model in range(48):
 box=(model%8*48,model//8*72,model%8*48+48,model//8*72+72)
 assert hashlib.sha256(source.crop(box).tobytes()).hexdigest()==manifest[model]['rgba_sha256']
 assert set(mask.crop(box).tobytes())<=set(range(5))
 count=len(set(palette[model*256:model*256+256])-{0});assert count>32,(model,count)
 counts.append(count)
# Hand-reviewed, independent landmarks: tailgates and window dividers are
# enamel; glass, seats, engine hardware and rider helmets are protected.
probes=((0,24,40,1),(0,24,23,0),(0,24,64,1),(6,24,56,1),(6,24,38,0),
 (15,24,59,1),(17,24,59,1),(24,24,60,1),(24,24,47,0),(31,24,63,1),
 (32,24,37,1),(33,24,39,0),(36,24,31,1),(36,24,57,0),(38,24,51,0),
 (40,24,49,1),(42,24,34,1),(42,24,52,0),(43,24,28,0),(47,24,12,1),(47,24,36,0))
for model,x,y,paint in probes:
 role=mask.getpixel((model%8*48+x,model//8*72+y))
 assert (role==1)==bool(paint),(model,x,y,role)
nm=Path('/workspace/gba-tools/sysroot/usr/bin/arm-none-eabi-nm')
symbols=subprocess.check_output([str(nm),str(ROOT/'build/arm9.elf')],text=True)
def address(name):return int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' '+name)),16)
assert address('__bss_end__')<0x02380000
assert address('__itcm_end')<=0x01008000
assert not any(line.endswith(' '+name) for line in symbols.splitlines() for name in ('overhead_roles','overhead_indexed','vehicle_roles','vehicle_indexed'))
result={'sha256':hashlib.sha256((ROOT/'build/Coastline.nds').read_bytes()).hexdigest(),
 'models':48,'source_hashes_match':True,'minimum_palette_colors':min(counts),'material_landmarks':len(probes),
 'ds_main_ram_end':hex(address('__bss_end__')),'ds_itcm_end':hex(address('__itcm_end')),
 'native_oracles_excluded_from_rom':True,'reviewed_body_and_protected_landmarks_pass':True}
(a.output/'openworld-art-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
