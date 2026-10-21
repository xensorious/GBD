#!/usr/bin/env python3
"""Build a streamed, indexed-color scene archive. No art is linked into main RAM."""
from pathlib import Path
import hashlib,json,struct
from PIL import Image,ImageOps
from story_script import CAST,PEOPLE,POSES,CATALOG,STAGES,load_story
def isolate_portrait(tile):
 """Keep the actor, excluding disconnected fragments from adjacent atlas cells."""
 alpha=tile.getchannel('A');raw=alpha.tobytes();w,h=tile.size
 remaining=bytearray(1 if v>=96 else 0 for v in raw);largest=[]
 for seed in range(w*h):
  if not remaining[seed]:continue
  remaining[seed]=0;stack=[seed];component=[]
  while stack:
   p=stack.pop();component.append(p);x=p%w
   for q in ((p-1 if x else -1),(p+1 if x+1<w else -1),p-w,p+w):
    if 0<=q<w*h and remaining[q]:remaining[q]=0;stack.append(q)
  if len(component)>len(largest):largest=component
 clean=bytearray(w*h)
 for p in largest:clean[p]=raw[p]
 tile.putalpha(Image.frombytes('L',(w,h),bytes(clean)));return tile
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/nitro';OUT.mkdir(parents=True,exist_ok=True)
portrait_size=int(CATALOG['portrait_size'])
assert portrait_size==192-24, 'Portraits fill the screen below the scene header'
backgrounds,scenes=load_story(ROOT)
nbg=len(backgrounds)
assets=[];portraits=[];portrait_map=[[None]*len(PEOPLE) for _ in POSES]
for spec in CATALOG['sheets']:
 for n in range(spec['people']):
  portrait_map[POSES.index(spec['pose'])][spec['first']+n]=len(portraits)
  portraits.append({'sheet':spec['file'],'grid':spec['grid'],'tile':n})
for row in portrait_map:
 for i,value in enumerate(row):
  if value is None:row[i]=portrait_map[0][i]
assert all(all(v is not None for v in row) for row in portrait_map)
for idx,spec in enumerate(backgrounds+portraits):
 sheet=Image.open(ROOT/'art/story'/spec['sheet']).convert('RGB' if idx<nbg else 'RGBA');cols,rows=spec['grid'];n=spec['tile']
 box=(round(n%cols*sheet.width/cols),round(n//cols*sheet.height/rows),round((n%cols+1)*sheet.width/cols),round((n//cols+1)*sheet.height/rows))
 tile=sheet.crop(box)
 if idx<nbg:tile=tile.resize((512,216),Image.Resampling.LANCZOS)
 else:
  tile=isolate_portrait(tile)
  # Prepare the final speaking size directly from the original source. Runtime
  # draws this canvas 1:1; only secondary actors and small UI portraits shrink.
  bounds=tile.getchannel('A').point(lambda a:255 if a>=96 else 0).getbbox();assert bounds,(idx,spec)
  figure=ImageOps.contain(tile.crop(bounds),(portrait_size-4,portrait_size-2),Image.Resampling.LANCZOS)
  tile=Image.new('RGBA',(portrait_size,portrait_size));tile.alpha_composite(figure,((portrait_size-figure.width)//2,portrait_size-figure.height))
 # Palette zero is transparent only for people; backgrounds retain all 256 colors.
 q=tile.convert('RGB').quantize(colors=256 if idx<nbg else 255,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.FLOYDSTEINBERG)
 p=q.getpalette();palette=[0]*256
 shift=0 if idx<nbg else 1
 for i in range(256-shift):
  r,g,b=p[i*3:i*3+3];palette[i+shift]=0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10)
 pixels=bytearray(q.tobytes())
 if idx>=nbg:
  alpha=tile.getchannel('A').tobytes();pixels=bytearray(value+1 if alpha[i]>=96 else 0 for i,value in enumerate(pixels))
 assets.append((tile.width,tile.height,struct.pack('<256H',*palette)+pixels))
# The table layout has a stable, bounded header. Individual records are streamed.
event_table=64;asset_table=event_table+125*12;data=bytearray(asset_table+len(assets)*16)
for index,scene in enumerate(scenes):
 lines=scene['lines'];assert 4<=len(lines)<=16,(scene['id'],len(lines))
 start=len(data);data.extend(b'\0'*(4+4*len(lines)));struct.pack_into('<HH',data,start,len(lines),0)
 for n,l in enumerate(lines):
  offset=len(data);struct.pack_into('<I',data,start+4+n*4,offset)
  text=l['text'].encode('ascii');focus=l['focus']|(0x8000 if l['move'] else 0)|(0x4000 if l['caption_right'] else 0)|(l['pose']<<9)|(l['companion_pose']<<11)
  data.extend(struct.pack('<BBBBHH',l['speaker'],l['companion'],l['stage'],l['background'],focus,len(text))+text)
 struct.pack_into('<IHHI',data,event_table+index*12,start,len(data)-start,len(lines),0)
for idx,(w,h,payload) in enumerate(assets):
 while len(data)%512:data.append(0)
 offset=len(data);data.extend(payload);struct.pack_into('<IHHII',data,asset_table+idx*16,offset,w,h,w*h,int(idx>=nbg))
struct.pack_into('<8s6I',data,0,b'CSTORY02',event_table,len(scenes),asset_table,len(assets),len(data),0)
(OUT/'story.bin').write_bytes(data)
line_count=sum(len(s['lines']) for s in scenes)
(ROOT/'build/story-manifest.json').write_text(json.dumps({'archive_sha256':hashlib.sha256(data).hexdigest(),'scenes':len(scenes),'dialogue_lines':line_count,'assets':len(assets),'backgrounds':nbg,'portraits':len(portraits),'portrait_size':portrait_size,'people':len(PEOPLE),'portrait_map':portrait_map,'bytes':len(data),'methods':STAGES,'events':[{'id':s['id'],'lines':len(s['lines']),'shots':s['shots']} for s in scenes]},indent=2)+'\n')
limits={'BACKGROUNDS':nbg,'PEOPLE':len(PEOPLE),'SPEAKERS':len(CAST),'PORTRAITS':len(portraits),'PORTRAIT_SIZE':portrait_size,'POSES':len(POSES),'ASSETS':len(assets),'LINES':line_count}
(ROOT/'build/story_limits.h').write_text('/* Generated from the authored story catalog. */\n'+''.join(f'#define STORY_{k} {v}\n' for k,v in limits.items()))
def cstring(value):return json.dumps(value,ensure_ascii=True)
visuals='/* Generated visual catalog; included by the dialogue renderer only. */\n'
visuals+='static const uint8_t story_portraits[STORY_POSES][STORY_PEOPLE] = {'+','.join('{'+','.join(map(str,row))+'}' for row in portrait_map)+'};\n'
visuals+='static const char*const story_locations[STORY_BACKGROUNDS] = {'+','.join(cstring(b.get('location','')) for b in backgrounds)+'};\n'
visuals+='static const char*const story_subjects[STORY_BACKGROUNDS][2] = {'+','.join('{'+','.join(cstring(x[1]) for x in list(b['subjects'].values())[:2])+'}' for b in backgrounds)+'};\n'
(ROOT/'build/story_visuals.h').write_text(visuals)
print('Story archive:',len(scenes),'scenes,',sum(len(s['lines']) for s in scenes),'lines,',len(assets),'indexed assets,',len(data),'ROM bytes')
