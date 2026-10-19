#!/usr/bin/env python3
"""Verify authored semantic pixels against source art and packed steering poses."""
from pathlib import Path
import argparse,array,hashlib,json,sys
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import build_assets as b
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
folder=ROOT/'art/materials';atlas=Image.open(folder/'vehicle-materials.png');manifest=json.loads((folder/'vehicle-materials.json').read_text())
assert atlas.size==(384,432) and len(manifest['models'])==48
# Landmarks chosen from the actual painted panels and lamp lenses. These
# distinguish a red body from a red emitter without a hue/band heuristic.
landmarks={
 0:{1:[(9,46),(12,46),(15,50)],3:[(11,50)],0:[(24,42)]},
 6:{1:[(9,50),(15,52)],3:[(11,52)]},
 14:{1:[(24,50)],3:[(11,49)]},16:{1:[(24,42)],3:[(11,46)]},
 24:{1:[(24,43)],3:[(11,42),(24,25)]},31:{1:[(10,43)],3:[(12,47)]},
 32:{1:[(15,48)],3:[(18,47)]},38:{1:[(24,43),(24,46),(24,48)],3:[(11,40)],0:[(24,33)]},
 13:{1:[(24,25)]},20:{1:[(24,24)],0:[(24,25)]},21:{1:[(24,27)],0:[(24,36)]},23:{1:[(23,28)]},
 40:{1:[(24,22)],3:[(11,47)]},
}
probes=0
for model,groups in landmarks.items():
 for role,points in groups.items():
  for x,y in points:
   assert atlas.getpixel((model%8*48+x,model//8*72+y))==role,(model,x,y,role)
   probes+=1
# Pacer lenses follow nonrectangular outlines. Adjacent body probes above
# ensure a larger bounding rectangle cannot silently reappear around them.
for side in (range(0,24),range(24,48)):
 coords=[(x,y) for y in range(72) for x in side if atlas.getpixel((x,y))==3]
 assert coords
 box=(max(x for x,y in coords)-min(x for x,y in coords)+1)*(max(y for x,y in coords)-min(y for x,y in coords)+1)
 assert len(coords)<box
packed=(ROOT/'build/vehicle_roles.bin').read_bytes();indices=(ROOT/'build/vehicle_indexed.bin').read_bytes();flags=(ROOT/'build/vehicle_palette_flags.bin').read_bytes()
assert len(packed)==48*5*48*72
checks=0
for model,source in enumerate(b.vehicle_sources()[0]):
 neutral=b.vehicle_pose(source,0);info=manifest['models'][model]
 assert hashlib.sha256(neutral.tobytes()).hexdigest()==info['rgba_sha256']
 mask=Image.frombytes('L',(48,72),atlas.crop((model%8*48,model//8*72,model%8*48+48,model//8*72+72)).tobytes())
 assert all(0<=role<=3 for role in mask.tobytes())
 for role,(_,_,_,alpha) in zip(mask.tobytes(),b.pixels(neutral)):
  if alpha<b.ALPHA:assert role==0
 for pose,offset in enumerate(b.YAW_FRONT_OFFSETS):
  slope=offset/(neutral.info['yaw_height']-1)
  mapped=mask.transform((48,72),Image.Transform.AFFINE,(1,slope,-offset/2-slope*neutral.info['yaw_top'],0,1,0),resample=Image.Resampling.NEAREST).tobytes()
  art=b.vehicle_pose(source,offset);base=(model*5+pose)*48*72;coverage=[0]*4
  for at,(role,(_,_,_,alpha)) in enumerate(zip(mapped,b.pixels(art))):
   expected=role if alpha>=b.ALPHA else 0
   assert packed[base+at]==expected,(model,pose,at,'role alignment')
   tag=flags[model*256+indices[base+at]]
   assert tag&3==(expected if expected<3 else 0),(model,pose,at,'material')
   if expected!=1:assert not tag&0xfc,(model,pose,at,'decal on protected part')
   for pattern in range(4):coverage[pattern]+=bool(tag&(1<<(pattern+2)))
   checks+=1
  assert all(coverage),(model,pose,'empty stencil')
rom=ROOT/'build/Coastline.nds';result={'sha256':hashlib.sha256(rom.read_bytes()).hexdigest(),'models_reviewed':48,'poses_reviewed':240,'fitted_liveries':960,'semantic_pixels_checked':checks,'semantic_landmarks':probes,'pacer_lenses_follow_nonrectangular_outlines':True,'source_art_hashes_match':True,'packed_roles_follow_artwork_transform':True,'decals_only_on_painted_pixels':True}
(a.output/'material-mask-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
