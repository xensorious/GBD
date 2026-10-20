#!/usr/bin/env python3
"""Build a streamed, indexed-color scene archive. No art is linked into main RAM."""
from pathlib import Path
import hashlib,json,re,struct
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/nitro';OUT.mkdir(parents=True,exist_ok=True)
CAST=['NIA','JULES','CALDER','IMANI','MARISOL','KADE','ROOK','REN','ASTER','SABLE','VOSS','MINA','YOU','NARRATOR']
BACKGROUNDS=['GARAGE','MARINA','MEET','FOUNDRY','PADDOCK','FINALE'];STAGES=['FACE','PAN','RADIO','MONTAGE']
scenes=[];current=None;shot=None
for lineno,line in enumerate((ROOT/'story/rising_tide.txt').read_text().splitlines(),1):
 line=line.strip()
 if not line or line.startswith('#'):continue
 if line.startswith('@'):
  key,bg,stage=line[1:].split();index='DCBAS'.index(key[0])*25+int(key[1:])-1
  assert index==len(scenes),(lineno,key,index,len(scenes))
  current={'id':key,'lines':[]};scenes.append(current);shot=(BACKGROUNDS.index(bg),STAGES.index(stage),128)
 elif line.startswith('>'):
  bg,stage,focus=line[1:].split();shot=(BACKGROUNDS.index(bg),STAGES.index(stage),int(focus))
 else:
  who,text=line.split(':',1);text=text.strip();speaker=CAST.index(who)
  assert text.isascii() and 1<=len(text)<192,(lineno,len(text),text)
  bg,stage,focus=shot
  if speaker==13:stage=3
  current['lines'].append({'speaker':speaker,'text':text,'background':bg,'stage':stage,'focus':max(0,min(256,focus+(speaker%3-1)*42))})
assert len(scenes)==125,len(scenes)
assets=[];sheets={}
bg=Image.open(ROOT/'art/story/backgrounds.png').convert('RGB');cast=Image.open(ROOT/'art/story/cast.png').convert('RGBA')
for idx in range(18):
 sheet=bg if idx<6 else cast;cols,rows=(2,3) if idx<6 else (4,3);n=idx if idx<6 else idx-6
 box=(round(n%cols*sheet.width/cols),round(n//cols*sheet.height/rows),round((n%cols+1)*sheet.width/cols),round((n//cols+1)*sheet.height/rows))
 tile=sheet.crop(box).resize((512,216) if idx<6 else (128,128),Image.Resampling.LANCZOS)
 # Palette zero is transparent only for people; backgrounds retain all 256 colors.
 q=tile.convert('RGB').quantize(colors=256 if idx<6 else 255,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.FLOYDSTEINBERG)
 p=q.getpalette();palette=[0]*256
 shift=0 if idx<6 else 1
 for i in range(256-shift):
  r,g,b=p[i*3:i*3+3];palette[i+shift]=0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10)
 pixels=bytearray(q.tobytes())
 if idx>=6:
  alpha=tile.getchannel('A').tobytes();pixels=bytearray(value+1 if alpha[i]>=96 else 0 for i,value in enumerate(pixels))
 assets.append((tile.width,tile.height,struct.pack('<256H',*palette)+pixels));sheets[idx]=tile
# The table layout has a stable, bounded header. Individual records are streamed.
event_table=64;asset_table=event_table+125*12;data=bytearray(asset_table+18*16)
for index,scene in enumerate(scenes):
 lines=scene['lines'];assert 4<=len(lines)<=16,(scene['id'],len(lines))
 actors=list(dict.fromkeys(l['speaker'] for l in lines if l['speaker']<12))
 start=len(data);data.extend(b'\0'*(4+4*len(lines)));struct.pack_into('<HH',data,start,len(lines),0)
 previous=actors[0] if actors else 0
 for n,l in enumerate(lines):
  speaker=l['speaker'];other=previous if previous!=speaker else next((a for a in actors if a!=speaker),255)
  if speaker<12:previous=speaker
  offset=len(data);struct.pack_into('<I',data,start+4+n*4,offset)
  text=l['text'].encode('ascii');data.extend(struct.pack('<BBBBHH',speaker,other,l['stage'],l['background'],l['focus'],len(text))+text)
 struct.pack_into('<IHHI',data,event_table+index*12,start,len(data)-start,len(lines),0)
for idx,(w,h,payload) in enumerate(assets):
 while len(data)%512:data.append(0)
 offset=len(data);data.extend(payload);struct.pack_into('<IHHII',data,asset_table+idx*16,offset,w,h,w*h,int(idx>=6))
struct.pack_into('<8s6I',data,0,b'CSTORY02',event_table,len(scenes),asset_table,len(assets),len(data),0)
(OUT/'story.bin').write_bytes(data)
(ROOT/'build/story-manifest.json').write_text(json.dumps({'archive_sha256':hashlib.sha256(data).hexdigest(),'scenes':len(scenes),'dialogue_lines':sum(len(s['lines']) for s in scenes),'assets':len(assets),'bytes':len(data),'methods':STAGES,'events':[{'id':s['id'],'lines':len(s['lines'])} for s in scenes]},indent=2)+'\n')
print('Story archive:',len(scenes),'scenes,',sum(len(s['lines']) for s in scenes),'lines,',len(assets),'indexed assets,',len(data),'ROM bytes')
