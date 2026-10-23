#!/usr/bin/env python3
"""Build a streamed, indexed-color scene archive. No art is linked into main RAM."""
from pathlib import Path
import hashlib,json,struct
from PIL import Image,ImageOps
from story_script import CAST,PEOPLE,POSES,CATALOG,STAGES,load_story
from story_proportions import compile_proportions
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

def remove_export_matte(tile):
 """Discard saturated red/yellow export fringe before silhouette extraction.

 This is opt-in for the approved replacement atlas, not a color correction for
 skin, tattoos, clothing or the other actors. These chroma-key edge pixels are
 outside the artwork and can otherwise survive the normal alpha cutoff.
 """
 pixels=[]
 for r,g,b,a in zip(*[iter(tile.tobytes())]*4):
  red=r>220 and g<60 and b<60
  yellow=r>220 and g>220 and b<60
  pixels.append((r,g,b,0 if red or yellow else a))
 tile.putdata(pixels)
 return tile
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/nitro';OUT.mkdir(parents=True,exist_ok=True)
portrait_width=int(CATALOG['portrait_width']);portrait_height=int(CATALOG['portrait_height'])
background_width,background_height=512,324
assert (portrait_width,portrait_height)==(256,264)
backgrounds,scenes=load_story(ROOT)
nbg=len(backgrounds)
tall=json.loads((ROOT/'art/story/tall-scenes.json').read_text())['backgrounds']
tall_by_name={b['name']:b for b in tall}
compiled_backgrounds=[dict(b,**{k:v for k,v in tall_by_name.get(b['name'],{}).items() if k!='name'}) for b in backgrounds]
def optional_catalog(relative):
 path=ROOT/'art/story'/relative
 return json.loads(path.read_text()) if path.exists() else {'characters':[]}
restyles=optional_catalog('restyle/catalog.json')['characters']
drafts=optional_catalog('new-cast/catalog.json')['characters']
quarters=optional_catalog('quarter/catalog.json')['characters']
restyle_by_id={person['character']:person for person in restyles}
assert len(restyle_by_id)==len(restyles) and 'NIA' not in restyle_by_id
assert 0<len(POSES)<=64
assets=[];portraits=[];sources=[[None]*len(PEOPLE) for _ in POSES]
for spec in CATALOG['sheets']:
 for n in range(spec['people']):
  sources[POSES.index(spec['pose'])][spec['first']+n]={'sheet':spec['file'],'grid':spec['grid'],'tile':n}
for spec in CATALOG.get('overrides',[]):
 actor=PEOPLE.index(spec['character']);pose=POSES.index(spec['pose'])
 sources[pose][actor]={k:v for k,v in spec.items() if k not in ('character','pose')}
for person in restyles:
 actor=PEOPLE.index(person['character'])
 assert {p['pose'] for p in person['portraits']}==set(POSES[:4]),person['character']
 for spec in person['portraits']:
  sources[POSES.index(spec['pose'])][actor]={k:v for k,v in spec.items() if k not in ('character','pose')}
for row in sources:
 for i,value in enumerate(row):
  if value is None:row[i]=sources[0][i]
assert all(all(v is not None for v in row) for row in sources)
# Resolve overrides before assigning IDs: replaced portraits consume no archive
# records, and unprovided expressions share each actor's existing default.
portrait_map=[];source_ids={}
for row in sources:
 ids=[]
 for spec in row:
  key=json.dumps(spec,sort_keys=True)
  if key not in source_ids:
   source_ids[key]=len(portraits);portraits.append(spec)
  ids.append(source_ids[key])
 portrait_map.append(ids)
ordinary_ids={value for row in portrait_map for value in row}
fullbody_map=[[-1]*len(PEOPLE) for _ in POSES]
for spec in CATALOG.get('fullbody_overrides',[]):
 actor=PEOPLE.index(spec['character']);pose=POSES.index(spec['pose'])
 source={k:v for k,v in spec.items() if k not in ('character','pose')}
 key=json.dumps(source,sort_keys=True)
 if key not in source_ids:source_ids[key]=len(portraits);portraits.append(source)
 fullbody_map[pose][actor]=source_ids[key]
fullbody_ids={v for row in fullbody_map for v in row if v>=0}
gallery=[]
for actor,(identifier,name) in enumerate(CATALOG['people']):
 profile=restyle_by_id.get(identifier,{})
 ids=[fullbody_map[p][actor] if fullbody_map[p][actor]>=0 else portrait_map[p][actor] for p in range(len(POSES))]
 gallery.append({'id':identifier,'name':name,'style':profile.get('style','Coastal workshop'),
  'description':profile.get('description',"Sunline's mechanic. Her approved look and expressions are preserved."),
  'draft':False,'portraits':ids,'full_body':[fullbody_map[p][actor]>=0 for p in range(len(POSES))]})
for person in drafts:
 assert person['id'] not in PEOPLE and person['full_body'] is True
 source={key:person[key] for key in ('sheet','bounds')}
 key=json.dumps(source,sort_keys=True)
 assert key not in source_ids,person['id']
 portrait=len(portraits);source_ids[key]=portrait;portraits.append(source)
 gallery.append({'id':person['id'],'name':person['name'],'style':person['style'],
  'description':person['description'],'draft':True,'portraits':[portrait]*len(POSES),'full_body':[True]*len(POSES)})
assert len({person['id'] for person in gallery})==len(gallery)
# Standing conversations keep the earlier artwork and add authored directions.
# Frame IDs stay below 256 even though the complete archive now has >256 assets.
quarter_sources={};quarter_entries=[]
for person in quarters:
 assert person['character'] in {entry['id'] for entry in gallery}
 expected=POSES if person['character']=='NIA' else POSES[:4] if person['character'] in PEOPLE else POSES[:1]
 assert {(p['pose'],p['facing']) for p in person['portraits']}=={(pose,direction) for pose in expected for direction in ('LEFT','RIGHT')},person['character']
 for spec in person['portraits']:
  source={key:spec[key] for key in ('sheet','bounds')}
  key=json.dumps(source,sort_keys=True)
  assert key not in source_ids,(person['character'],spec['pose'],spec['facing'])
  portrait=len(portraits);source_ids[key]=portrait;portraits.append(source)
  identity=(person['character'],spec['pose'],spec['facing'])
  assert identity not in quarter_sources,identity
  quarter_sources[identity]=portrait
  quarter_entries.append({'character':person['character'],'pose':spec['pose'],'facing':spec['facing'],'portrait':portrait,**source})
quarter_available=bool(quarters)
if quarter_available:
 assert {p['character'] for p in quarters}=={entry['id'] for entry in gallery}
 assert len(quarter_entries)==142
quarter_map=[[[-1]*len(PEOPLE) for _ in POSES] for _ in range(2)]
gallery_quarters=[]
for actor,person in enumerate(gallery):
 rows=[]
 for pose in POSES:
  ids=[quarter_sources.get((person['id'],pose,direction),quarter_sources.get((person['id'],'DEFAULT',direction),-1)) for direction in ('LEFT','RIGHT')]
  rows.append(ids)
  if actor<len(PEOPLE):
   for facing,portrait in enumerate(ids):quarter_map[facing][POSES.index(pose)][actor]=portrait
 gallery_quarters.append(rows)
assert len(portraits)<256
for person in gallery:
 assert all(person[key].isascii() for key in ('name','style','description')),person['id']
gallery_pose_counts=[len(set(person['portraits'])) for person in gallery]
# Player replies do not change the people physically present in a shot. Store
# the otherwise omitted partner beside the visual catalog, leaving dialogue and
# save encoding untouched. In larger groups, retain the previously visible pair.
reply_pairs=[];stage_offsets=[0]
for scene in scenes:
 last_shot=-1;visible=[];held_poses={}
 for line in scene['lines']:
  shot=scene['shots'][line['shot']]
  if line['shot']!=last_shot:
   last_shot=line['shot'];visible=[]
   held_poses={PEOPLE.index(actor):pose for actor,pose in shot['poses'].items()}
  speaker,companion=line['speaker'],line['companion'];cue=0
  if line['stage']==0:
   if speaker<len(PEOPLE):
    visible=[speaker]+([companion] if companion<len(PEOPLE) and companion!=speaker else [])
    held_poses[speaker]=line['pose']
    if companion<len(PEOPLE):held_poses[companion]=line['companion_pose']
   elif companion<len(PEOPLE):
    others=[actor for actor in visible if actor!=companion]
    if not others:others=[PEOPLE.index(actor) for actor in shot['present'] if PEOPLE.index(actor)!=companion]
    if others:
     other=others[0];other_pose=held_poses[other]
     assert 0<=other_pose<8 and 0<=line['companion_pose']<8
     cue=(companion+1)|((other+1)<<4)|(other_pose<<8)|(line['companion_pose']<<11)
     visible=[companion,other];held_poses[companion]=line['companion_pose']
  reply_pairs.append(cue)
 stage_offsets.append(len(reply_pairs))
assert len(reply_pairs)==sum(len(scene['lines']) for scene in scenes)
for idx,spec in enumerate(compiled_backgrounds+portraits):
 sheet=Image.open(ROOT/'art/story'/spec['sheet']).convert('RGB' if idx<nbg else 'RGBA')
 if 'bounds' in spec:box=tuple(spec['bounds'])
 else:
  cols,rows=spec['grid'];n=spec['tile']
  box=(round(n%cols*sheet.width/cols),round(n//cols*sheet.height/rows),round((n%cols+1)*sheet.width/cols),round((n//cols+1)*sheet.height/rows))
 assert 0<=box[0]<box[2]<=sheet.width and 0<=box[1]<box[3]<=sheet.height,(spec,box,sheet.size)
 tile=sheet.crop(box)
 if idx<nbg:
  # Reframe older close shots with a consistent lens, preserving aspect ratio.
  # The standing-height paintings already include the new foreground floor.
  if spec.get('plate'):
   # One finished illustration occupies the two-LCD picture area. Prepare it
   # directly from the source once, with no runtime zoom or character overlays.
   painting=ImageOps.fit(tile,(256,288),Image.Resampling.LANCZOS)
   tile=Image.new('RGB',(background_width,background_height),(5,10,16))
   tile.paste(painting,((background_width-256)//2,(background_height-288)//2))
  else:
   centering=(0.25,0.5) if spec['name']=='LIGHTHOUSE' else (0.5,0.5)
   tile=ImageOps.fit(tile,(background_width,background_height),Image.Resampling.LANCZOS,centering=centering)
 else:
  if spec.get('remove_export_matte'):tile=remove_export_matte(tile)
  tile=isolate_portrait(tile)
  # Prepare the final speaking size directly from the original source. Runtime
  # draws this canvas 1:1; only secondary actors and small UI portraits shrink.
  bounds=tile.getchannel('A').point(lambda a:255 if a>=96 else 0).getbbox();assert bounds,(idx,spec)
  figure=ImageOps.contain(tile.crop(bounds),(portrait_width-4,portrait_height-2),Image.Resampling.LANCZOS)
  tile=Image.new('RGBA',(portrait_width,portrait_height));tile.alpha_composite(figure,((portrait_width-figure.width)//2,portrait_height-figure.height))
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
# A character's arm gesture must not change the pair's camera scale. Union the
# final pixel bounds for each authored character/direction once at build time.
pose_envelopes=[]
for w,h,payload in assets[nbg:]:
 bounds=Image.frombytes('L',(w,h),payload[512:]).getbbox();assert bounds
 pose_envelopes.append([bounds[0],bounds[2]])
for person in quarters:
 for facing in ('LEFT','RIGHT'):
  ids=[quarter_sources[(person['character'],pose['pose'],facing)] for pose in person['portraits'] if pose['facing']==facing]
  envelope=[min(pose_envelopes[i][0] for i in ids),max(pose_envelopes[i][1] for i in ids)]
  for i in ids:pose_envelopes[i]=envelope[:]
proportion_profile=json.loads((ROOT/'art/story/proportions.json').read_text())
proportions=compile_proportions(proportion_profile,gallery,quarter_entries,assets,nbg,pose_envelopes,len(PEOPLE))
# The table layout has a stable, bounded header. Individual records are streamed.
event_table=64;asset_table=event_table+125*12;data=bytearray(asset_table+len(assets)*16)
for index,scene in enumerate(scenes):
 lines=scene['lines'];assert 4<=len(lines)<=16,(scene['id'],len(lines))
 start=len(data);data.extend(b'\0'*(4+4*len(lines)));struct.pack_into('<HH',data,start,len(lines),0)
 for n,l in enumerate(lines):
  offset=len(data);struct.pack_into('<I',data,start+4+n*4,offset)
  text=l['text'].encode('ascii');focus=l['focus']|(0x8000 if l['move'] else 0)|(0x4000 if l['caption_right'] else 0)|((l['pose']&3)<<9)|((l['companion_pose']&3)<<11)
  pose_flags=(l['pose']>>2)|((l['companion_pose']>>2)<<4)
  data.extend(struct.pack('<BBBBHBB',l['speaker'],l['companion'],l['stage'],l['background'],focus,len(text),pose_flags)+text)
 struct.pack_into('<IHHI',data,event_table+index*12,start,len(data)-start,len(lines),0)
for idx,(w,h,payload) in enumerate(assets):
 while len(data)%512:data.append(0)
 offset=len(data);data.extend(payload);struct.pack_into('<IHHII',data,asset_table+idx*16,offset,w,h,w*h,int(idx>=nbg))
struct.pack_into('<8s6I',data,0,b'CSTORY02',event_table,len(scenes),asset_table,len(assets),len(data),0)
(OUT/'story.bin').write_bytes(data)
line_count=sum(len(s['lines']) for s in scenes)
(ROOT/'build/story-manifest.json').write_text(json.dumps({'archive_sha256':hashlib.sha256(data).hexdigest(),'scenes':len(scenes),'dialogue_lines':line_count,'assets':len(assets),'backgrounds':nbg,'portraits':len(portraits),'ordinary_portraits':len(ordinary_ids),'portrait_size':portrait_width,'portrait_width':portrait_width,'portrait_height':portrait_height,'background_width':background_width,'background_height':background_height,'fullbody_map':fullbody_map,'fullbody_portraits':len(fullbody_ids),'fullbody_backgrounds':[b['name'] for b in tall if b.get('full_body')],'people':len(PEOPLE),'portrait_map':portrait_map,'gallery':gallery,'gallery_count':len(gallery),'restyled_people':list(restyle_by_id),'proportions':proportions,'pose_envelopes':pose_envelopes,'reply_pairs':reply_pairs,'stage_offsets':stage_offsets,'quarter_portraits':len(quarter_entries),'quarter_entries':quarter_entries,'quarter_map':quarter_map,'gallery_quarters':gallery_quarters,'bytes':len(data),'methods':STAGES,'events':[{'id':s['id'],'lines':len(s['lines']),'shots':s['shots']} for s in scenes]},indent=2)+'\n')
limits={'BACKGROUNDS':nbg,'BACKGROUND_WIDTH':background_width,'BACKGROUND_HEIGHT':background_height,'PEOPLE':len(PEOPLE),'SPEAKERS':len(CAST),'PORTRAITS':len(portraits),'PORTRAIT_SIZE':portrait_width,'PORTRAIT_WIDTH':portrait_width,'PORTRAIT_HEIGHT':portrait_height,'POSES':len(POSES),'ASSETS':len(assets),'LINES':line_count,'GALLERY_COUNT':len(gallery)}
advances=(ROOT/'build/font_advance.bin').read_bytes();assert len(advances)==96
(ROOT/'build/story_limits.h').write_text('/* Generated from the authored story catalog and actual dialogue font. */\n'+''.join(f'#define STORY_{k} {v}\n' for k,v in limits.items())+'#define STORY_FONT_ADVANCE_INITIALIZER {'+','.join(map(str,advances))+'}\n#define STORY_GALLERY_POSE_COUNTS_INITIALIZER {'+','.join(map(str,gallery_pose_counts))+'}\n#define STORY_QUARTER_AVAILABLE '+str(int(quarter_available))+'\n')
def cstring(value):return json.dumps(value,ensure_ascii=True)
visuals='/* Generated visual catalog; included by the dialogue renderer only. */\n'
visuals+='#define STORY_POSE_ENVELOPES_AVAILABLE 1\n#define STORY_REPLY_PAIRS_AVAILABLE 1\n'
visuals+='#define STORY_CHARACTER_SCALES_AVAILABLE 1\n#define STORY_STANDING_MARGIN '+str(proportions['standing_margin'])+'\n'
visuals+='static const uint16_t story_character_scales[STORY_GALLERY_COUNT] = {'+','.join(map(str,proportions['character_scales']))+'};\n'
visuals+='static const int8_t story_portrait_characters[STORY_PORTRAITS] = {'+','.join(map(str,proportions['portrait_characters']))+'};\n'
visuals+='static const uint16_t story_portrait_scales[STORY_PORTRAITS] = {'+','.join(map(str,proportions['portrait_scales']))+'};\n'
visuals+='static const uint16_t story_pair_camera_scales[STORY_PEOPLE][STORY_PEOPLE] = {'+','.join('{'+','.join(map(str,row))+'}' for row in proportions['pair_camera_scales'])+'};\n'
visuals+='static const uint16_t story_pose_envelopes[STORY_PORTRAITS][2] = {'+','.join('{'+','.join(map(str,row))+'}' for row in pose_envelopes)+'};\n'
visuals+='static const uint16_t story_stage_offsets[STORY_TOTAL+1] = {'+','.join(map(str,stage_offsets))+'};\n'
visuals+='static const uint16_t story_reply_pairs[STORY_LINES] = {'+','.join(map(str,reply_pairs))+'};\n'
visuals+='static const uint8_t story_portraits[STORY_POSES][STORY_PEOPLE] = {'+','.join('{'+','.join(map(str,row))+'}' for row in portrait_map)+'};\n'
visuals+='static const int16_t story_fullbodies[STORY_POSES][STORY_PEOPLE] = {'+','.join('{'+','.join(map(str,row))+'}' for row in fullbody_map)+'};\n'
visuals+='static const int16_t story_quarter_portraits[2][STORY_POSES][STORY_PEOPLE] = {'+','.join('{'+','.join('{'+','.join(map(str,row))+'}' for row in direction)+'}' for direction in quarter_map)+'};\n'
visuals+='static const int16_t story_gallery_quarters[STORY_GALLERY_COUNT][STORY_POSES][2] = {'+','.join('{'+','.join('{'+','.join(map(str,row))+'}' for row in person)+'}' for person in gallery_quarters)+'};\n'
for field in ('names','styles','descriptions'):
 key={'names':'name','styles':'style','descriptions':'description'}[field]
 visuals+='static const char*const story_gallery_'+field+'[STORY_GALLERY_COUNT] = {'+','.join(cstring(person[key]) for person in gallery)+'};\n'
visuals+='static const int16_t story_gallery_portraits[STORY_GALLERY_COUNT][STORY_POSES] = {'+','.join('{'+','.join(map(str,person['portraits']))+'}' for person in gallery)+'};\n'
visuals+='static const uint8_t story_gallery_fullbody[STORY_GALLERY_COUNT][STORY_POSES] = {'+','.join('{'+','.join(str(int(value)) for value in person['full_body'])+'}' for person in gallery)+'};\n'
visuals+='static const uint8_t story_gallery_draft[STORY_GALLERY_COUNT] = {'+','.join(str(int(person['draft'])) for person in gallery)+'};\n'
visuals+='static const uint8_t story_fullbody_background[STORY_BACKGROUNDS] = {'+','.join(str(int(b['name'] in tall_by_name and tall_by_name[b['name']].get('full_body',False))) for b in backgrounds)+'};\n'
visuals+='static const uint16_t story_ground_y[STORY_BACKGROUNDS] = {'+','.join(str(tall_by_name.get(b['name'],{}).get('ground_y',284)) for b in backgrounds)+'};\n'
visuals+='static const char*const story_locations[STORY_BACKGROUNDS] = {'+','.join(cstring(b.get('location','')) for b in backgrounds)+'};\n'
visuals+='static const char*const story_subjects[STORY_BACKGROUNDS][2] = {'+','.join('{'+','.join(cstring(x[1]) for x in list(b['subjects'].values())[:2])+'}' for b in backgrounds)+'};\n'
(ROOT/'build/story_visuals.h').write_text(visuals)
print('Story archive:',len(scenes),'scenes,',sum(len(s['lines']) for s in scenes),'lines,',len(assets),'indexed assets,',len(data),'ROM bytes')
