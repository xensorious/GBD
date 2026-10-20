#!/usr/bin/env python3
"""Build a streamed, indexed-color scene archive. No art is linked into main RAM."""
from pathlib import Path
import hashlib,json,struct
from PIL import Image
from story_script import CAST,STAGES,load_story
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/nitro';OUT.mkdir(parents=True,exist_ok=True)
backgrounds,scenes=load_story(ROOT)
nbg=len(backgrounds)
assets=[];sheets={}
cast=Image.open(ROOT/'art/story/cast.png').convert('RGBA')
for idx in range(nbg+12):
 if idx<nbg:
  spec=backgrounds[idx];sheet=Image.open(ROOT/'art/story'/spec['sheet']).convert('RGB');cols,rows=spec['grid'];n=spec['tile']
 else:sheet=cast;cols,rows=4,3;n=idx-nbg
 box=(round(n%cols*sheet.width/cols),round(n//cols*sheet.height/rows),round((n%cols+1)*sheet.width/cols),round((n//cols+1)*sheet.height/rows))
 tile=sheet.crop(box).resize((512,216) if idx<nbg else (128,128),Image.Resampling.LANCZOS)
 # Palette zero is transparent only for people; backgrounds retain all 256 colors.
 q=tile.convert('RGB').quantize(colors=256 if idx<nbg else 255,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.FLOYDSTEINBERG)
 p=q.getpalette();palette=[0]*256
 shift=0 if idx<nbg else 1
 for i in range(256-shift):
  r,g,b=p[i*3:i*3+3];palette[i+shift]=0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10)
 pixels=bytearray(q.tobytes())
 if idx>=nbg:
  alpha=tile.getchannel('A').tobytes();pixels=bytearray(value+1 if alpha[i]>=96 else 0 for i,value in enumerate(pixels))
 assets.append((tile.width,tile.height,struct.pack('<256H',*palette)+pixels));sheets[idx]=tile
# The table layout has a stable, bounded header. Individual records are streamed.
event_table=64;asset_table=event_table+125*12;data=bytearray(asset_table+len(assets)*16)
for index,scene in enumerate(scenes):
 lines=scene['lines'];assert 4<=len(lines)<=16,(scene['id'],len(lines))
 start=len(data);data.extend(b'\0'*(4+4*len(lines)));struct.pack_into('<HH',data,start,len(lines),0)
 for n,l in enumerate(lines):
  offset=len(data);struct.pack_into('<I',data,start+4+n*4,offset)
  text=l['text'].encode('ascii');focus=l['focus']|(0x8000 if l['move'] else 0)|(0x4000 if l['caption_right'] else 0)
  data.extend(struct.pack('<BBBBHH',l['speaker'],l['companion'],l['stage'],l['background'],focus,len(text))+text)
 struct.pack_into('<IHHI',data,event_table+index*12,start,len(data)-start,len(lines),0)
for idx,(w,h,payload) in enumerate(assets):
 while len(data)%512:data.append(0)
 offset=len(data);data.extend(payload);struct.pack_into('<IHHII',data,asset_table+idx*16,offset,w,h,w*h,int(idx>=nbg))
struct.pack_into('<8s6I',data,0,b'CSTORY02',event_table,len(scenes),asset_table,len(assets),len(data),0)
(OUT/'story.bin').write_bytes(data)
(ROOT/'build/story-manifest.json').write_text(json.dumps({'archive_sha256':hashlib.sha256(data).hexdigest(),'scenes':len(scenes),'dialogue_lines':sum(len(s['lines']) for s in scenes),'assets':len(assets),'backgrounds':nbg,'bytes':len(data),'methods':STAGES,'events':[{'id':s['id'],'lines':len(s['lines']),'shots':s['shots']} for s in scenes]},indent=2)+'\n')
print('Story archive:',len(scenes),'scenes,',sum(len(s['lines']) for s in scenes),'lines,',len(assets),'indexed assets,',len(data),'ROM bytes')
