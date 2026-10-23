#!/usr/bin/env python3
"""Independently check cartridge archive offsets, art masks and resident budgets."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from PIL import Image
from inspect_rom import inspect
from game_snapshot import find_symbol
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from story_script import load_story,CAST,PEOPLE,POSES,CATALOG,STAGES
from baked_catalog import approved_baked_pages, check_baked_payloads, LEGACY_BACKGROUNDS
source_backgrounds,source_scenes=load_story(ROOT)
baked=approved_baked_pages(ROOT,source_backgrounds,source_scenes)
manifest=json.loads((ROOT/'build/story-manifest.json').read_text());nbg=len(source_backgrounds);portrait_map=manifest['portrait_map'];portraits_seen=set();background_events={}
expected_lines=sum(len(e['lines']) for e in source_scenes)
portrait_width=manifest['portrait_width'];portrait_height=manifest['portrait_height']
background_width=manifest['background_width'];background_height=manifest['background_height']
assert (portrait_width,portrait_height)==(256,264)
assert (background_width,background_height)==(512,324)
scratch_required=background_width*background_height+2*portrait_width*portrait_height+3*256*2
assert scratch_required==302592
assert scratch_required<=307200
fullbody_map=manifest['fullbody_map']
assert len(portrait_map)==len(fullbody_map)==len(POSES)==5
assert all(len(row)==len(PEOPLE) for row in portrait_map+fullbody_map)
ordinary_ids={value for row in portrait_map for value in row}
fullbody_ids={value for row in fullbody_map for value in row if value>=0}
assert ordinary_ids==set(range(57)) and manifest['ordinary_portraits']==57
assert len(fullbody_ids)==manifest['fullbody_portraits']==5
assert not ordinary_ids&fullbody_ids
gallery=manifest['gallery']
assert len(PEOPLE)==14 and CAST==PEOPLE+['YOU','NARRATOR']
assert manifest['gallery_count']==len(gallery)==28
assert [entry['id'] for entry in gallery[:14]]==PEOPLE
assert [entry['name'] for entry in gallery[:14]]==[person[1] for person in CATALOG['people']]
assert all(not entry['draft'] for entry in gallery[:14]) and all(entry['draft'] for entry in gallery[14:])
assert len({entry['id'] for entry in gallery})==28
assert all(len(entry['portraits'])==len(entry['full_body'])==len(POSES) for entry in gallery)
gallery_ids={value for entry in gallery for value in entry['portraits']}
draft_ids={value for entry in gallery[14:] for value in entry['portraits']}
assert len(draft_ids)==14 and not draft_ids&(ordinary_ids|fullbody_ids)
assert all(len(set(entry['portraits']))==1 and entry['full_body']==[True]*5 for entry in gallery[14:])
legacy_ids=ordinary_ids|fullbody_ids|draft_ids
assert legacy_ids==set(range(76))
quarter_count=manifest.get('quarter_portraits',0)
quarter_entries=manifest.get('quarter_entries',[])
quarter_ids={entry['portrait'] for entry in quarter_entries}
assert quarter_count in (0,142) and len(quarter_entries)==quarter_count
assert quarter_ids==set(range(76,76+quarter_count))
assert legacy_ids|quarter_ids==set(range(manifest['portraits'])) and manifest['portraits']==76+quarter_count
if quarter_count:
 assert len(manifest['quarter_map'])==2 and all(len(direction)==5 and all(len(row)==14 for row in direction) for direction in manifest['quarter_map'])
 assert len(manifest['gallery_quarters'])==28 and all(len(person)==5 and all(len(row)==2 for row in person) for person in manifest['gallery_quarters'])
 assert {portrait for person in manifest['gallery_quarters'] for row in person for portrait in row}==quarter_ids
nia=PEOPLE.index('NIA')
assert len({row[nia] for row in fullbody_map})==5
assert all(value>=0 if actor==nia else value==-1 for row in fullbody_map for actor,value in enumerate(row))
assert (CATALOG['portrait_width'],CATALOG['portrait_height'])==(portrait_width,portrait_height)
fullbody_sources=CATALOG['fullbody_overrides']
assert len(fullbody_sources)==5 and {spec['character'] for spec in fullbody_sources}=={'NIA'}
assert {spec['pose'] for spec in fullbody_sources}==set(POSES)
for spec in fullbody_sources:
 with Image.open(ROOT/'art/story'/spec['sheet']) as image:
  assert 'A' in image.getbands(),spec
  left,top,right,bottom=spec['bounds'];assert 0<=left<right<=image.width and 0<=top<bottom<=image.height,spec
  mask=image.crop(spec['bounds']).getchannel('A').point(lambda value:255 if value>=96 else 0)
  assert mask.getbbox() and 0<mask.tobytes().count(0)<len(mask.tobytes())-1000,spec
tall=json.loads((ROOT/'art/story/tall-scenes.json').read_text())
fullbody_backgrounds=set(manifest['fullbody_backgrounds'])
assert fullbody_backgrounds=={spec['name'] for spec in tall['backgrounds'] if spec.get('full_body')}
assert len(fullbody_backgrounds)==8
assert fullbody_backgrounds<={spec['name'] for spec in source_backgrounds}
for spec in tall['backgrounds']:
 assert spec['full_body'] is True and spec['ground_y']==284
 with Image.open(ROOT/'art/story'/spec['sheet']) as image:assert image.size==(512,324)
fullbody_occurrences=[]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/story-release');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build/Coastline.nds';r=rom.read_bytes();fat,size=struct.unpack_from('<II',r,0x48);assert size==8
start,end=struct.unpack_from('<II',r,fat);archive=r[start:end];assert archive==(ROOT/'build/nitro/story.bin').read_bytes()
magic,event_table,events,asset_table,assets,total,unused=struct.unpack_from('<8s6I',archive)
assert magic==b'CSTORY02' and events==125 and assets==manifest['assets'] and total==len(archive)
lines=0;methods=set();speakers=set();backgrounds=set();longest=0;extended_leads=0;extended_companions=0
assert 5<=len(POSES)<=64
for event in range(events):
 offset,bytes_,count,reserved=struct.unpack_from('<IHHI',archive,event_table+event*12);n,pad=struct.unpack_from('<HH',archive,offset);assert count==n and 4<=n<=16
 assert offset+bytes_<=len(archive) and not pad and not reserved
 for line in range(n):
  ptr=struct.unpack_from('<I',archive,offset+4+line*4)[0];speaker,companion,method,bg,focus,length,pose_flags=struct.unpack_from('<BBBBHBB',archive,ptr)
  assert speaker<len(CAST) and (companion<len(PEOPLE) or companion==255) and method<len(STAGES) and bg<nbg and (focus&511)<=256 and not focus&0x2000 and 0<length<192
  pose=((focus>>9)&3)|((pose_flags&15)<<2);companion_pose=((focus>>11)&3)|((pose_flags>>4)<<2)
  assert pose<len(POSES) and companion_pose<len(POSES)
  text=archive[ptr+8:ptr+8+length].decode('ascii');assert '\0' not in text and len(text)==length
  authored=source_scenes[event]['lines'][line]
  expected_focus=authored['focus']|(0x8000 if authored['move'] else 0)|(0x4000 if authored['caption_right'] else 0)|((authored['pose']&3)<<9)|((authored['companion_pose']&3)<<11)
  expected_flags=(authored['pose']>>2)|((authored['companion_pose']>>2)<<4)
  assert (speaker,companion,method,bg,focus,pose_flags,text)==(authored['speaker'],authored['companion'],authored['stage'],authored['background'],expected_focus,expected_flags,authored['text'])
  assert (pose,companion_pose)==(authored['pose'],authored['companion_pose'])
  assert not any(token in text for token in ('TODO','TBD','Proposed objective','IDEA:'))
  methods.add(method);speakers.add(speaker);backgrounds.add(bg);lines+=1;longest=max(longest,length)
  background_events.setdefault(bg,set()).add(event)
  if speaker<len(PEOPLE):portraits_seen.add(portrait_map[pose][speaker])
  if method==0 and companion<len(PEOPLE):portraits_seen.add(portrait_map[companion_pose][companion])
  if method==0 and source_backgrounds[bg]['name'] in fullbody_backgrounds:
   for role,actor,selected in (('lead',speaker,pose),('companion',companion,companion_pose)):
    if actor<len(PEOPLE) and (role=='lead' or speaker>=len(PEOPLE)) and fullbody_map[selected][actor]>=0:
     fullbody_occurrences.append({'event':source_scenes[event]['id'],'line':line,'role':role,'portrait':fullbody_map[selected][actor]})
  extended_leads+=speaker==PEOPLE.index('NIA') and pose==4
  extended_companions+=method==0 and speaker<len(PEOPLE) and companion==PEOPLE.index('NIA') and companion_pose==4
assert lines==expected_lines and methods==set(range(len(STAGES))) and speakers==set(range(len(CAST))) and backgrounds==set(range(nbg))
assert portraits_seen<=ordinary_ids
# Eli and Vale now have extra outfits even when the unchanged script does not
# select every one. The gallery covers those new images; authored references
# must stay valid, and every archive portrait must have a story or gallery use.
assert portraits_seen|gallery_ids==legacy_ids
assert portraits_seen|gallery_ids|quarter_ids==set(range(manifest['portraits']))
assert {entry['portrait'] for entry in fullbody_occurrences}==fullbody_ids
assert extended_leads
assert extended_companions==sum(line['stage']==0 and line['speaker']<len(PEOPLE) and
                               line['companion']==PEOPLE.index('NIA') and line['companion_pose']==4
                               for scene in source_scenes for line in scene['lines'])
shot_count=0
for scene in source_scenes:
 for shot in scene['shots']:
  shot_count+=1
  # Independent projection check: the named source subject must be inside the
  # final crop, including the wider letterboxed montage lens.
  width=276 if shot['stage']==3 else 256
  left=128 if shot['stage']==4 else min(shot['focus'],512-width)
  assert left<=shot['subject_x']<left+width,(scene['id'],shot['subject'])
  if shot['stage']!=0:assert all(l['companion']==255 for l in shot['lines'])
assert shot_count==sum(len(e['shots']) for e in source_scenes)
important={'D01':{('BENCH','JACK'),('BENCH','KEYS')},
 'D10':{('MINA_BEDROOM','NOTES'),('MINA_BEDROOM','PHOTOS')},'D17':{('JULES_ROOM','HELMET'),('JULES_ROOM','BILLS')},
 'D19':{('CORNER','MARKER'),('CORNER','EXIT')},'D24':{('LIGHTHOUSE','PHOTO'),('LIGHTHOUSE','SEA')},
 'C07':{('RAIN','WATER'),('RAIN','AWNING')},'C10':{('HARBOR_BATHROOM','BASIN'),('HARBOR_BATHROOM','MIRROR')},
 'C19':{('WAITING_ROOM','CHAIRS'),('WAITING_ROOM','FLOWERS')},'B20':{('OLD_WORKSHOP','LIFT'),('OLD_WORKSHOP','CHAIR')},
 'A13':{('SKY_LOUNGE','FOLDER'),('SKY_LOUNGE','WINDOW')},'A21':{('OFFICE','CONTRACT'),('OFFICE','LICENSE')},
 'S20':{('OFFICE','CONTRACT'),('OFFICE','LICENSE')},'S25':{('NIGHTFINAL','CORNER'),('EPILOGUE','KEYS'),('EPILOGUE','CHAIRS')}}
for key,subjects in important.items():
 scene=next(s for s in source_scenes if s['id']==key)
 assert subjects<={(source_backgrounds[s['background']]['name'],s['subject']) for s in scene['shots']},key
for event,name in zip(baked['events'],('NIA_POOL_EVENING','NIA_REPLAY_NIGHT','NIA_SOFA_PLAN')):
 scene=next(s for s in source_scenes if s['id']==event)
 assert {source_backgrounds[shot['background']]['name'] for shot in scene['shots']}=={name}
art=[];baked_records={}
for i in range(assets):
 offset,w,h,size,kind=struct.unpack_from('<IHHII',archive,asset_table+i*16);assert size==w*h and offset+512+size<=len(archive)
 assert (w,h)==((background_width,background_height) if i<nbg else (portrait_width,portrait_height))
 assert kind==int(i>=nbg)
 palette=struct.unpack_from('<256H',archive,offset);pixels=archive[offset+512:offset+512+size]
 if kind:
  assert palette[0]==0 and 100<pixels.count(0)<size-1000
  # Large detached strips are atlas bleed, not hair highlights or tiny jewelry.
  remaining=bytearray(bool(v) for v in pixels);components=[]
  for seed in range(size):
   if not remaining[seed]:continue
   remaining[seed]=0;todo=[seed];area=0
   while todo:
    p=todo.pop();area+=1;x=p%w
    for q in ((p-1 if x else -1),(p+1 if x+1<w else -1),p-w,p+w):
     if 0<=q<size and remaining[q]:remaining[q]=0;todo.append(q)
   components.append(area)
  components.sort(reverse=True);assert len(components)<2 or components[1]<=32,(i,components[:5])
 else:assert all(c&32768 for c in palette)
 if LEGACY_BACKGROUNDS<=i<nbg:
  baked_records[i]={'width':w,'height':h,'kind':kind,'palette':archive[offset:offset+512],
                    'pixels':pixels,'sha256':hashlib.sha256(archive[offset:offset+512+size]).hexdigest()}
 art.append({'id':i,'width':w,'height':h,'transparent':bool(kind),'sha256':hashlib.sha256(pixels).hexdigest()})
baked_payloads=check_baked_payloads(baked,baked_records)
elf=ROOT/'build/arm9.elf';main_end=find_symbol(elf,'__bss_end__');itcm_end=find_symbol(elf,'__itcm_end')
assert main_end<0x02380000 and itcm_end<=0x01008000
result={'rom':inspect(rom),'archive_matches_nitrofs':True,'archive_bytes':len(archive),'scenes':events,'lines':lines,'backgrounds':nbg,'people':len(PEOPLE),'portraits':manifest['portraits'],'portrait_width':portrait_width,'portrait_height':portrait_height,'background_width':background_width,'background_height':background_height,'authored_portrait_references_valid':True,'authored_unique_ordinary_portraits':len(portraits_seen),'ordinary_portraits':len(ordinary_ids),'legacy_portraits':len(legacy_ids),'quarter_portraits':quarter_count,'quarter_directional_maps_valid':True,'gallery_people':len(gallery),'new_gallery_only_people':14,'draft_portraits_excluded_from_story':True,'all_portraits_have_story_or_gallery_use':True,'fullbody_portraits':len(fullbody_ids),'fullbody_backgrounds':sorted(fullbody_backgrounds),'fullbody_occurrences':fullbody_occurrences,'no_large_atlas_fragments':True,'one_event_new_backgrounds':sum(len(v)==1 for k,v in background_events.items() if k>=18),'new_place_events':len(set.union(*(v for k,v in background_events.items() if k>=18))),'authored_shots':shot_count,'compiled_directions_match_source':True,'subjects_in_final_frame':True,'shot_local_companions':True,'longest_line':longest,'methods':sorted(methods),'all_character_sprites':True,'portrait_alpha_masks_valid':True,'resident_end':hex(main_end),'resident_headroom_bytes':0x02380000-main_end,'itcm_headroom_bytes':0x01008000-itcm_end,'shared_scratch_bytes':307200,'story_scratch_required_bytes':scratch_required,'background_cache_slots':1,'actor_cache_slots':2,'art':art}
result.update({'pose_count':len(POSES),'extended_pose_lead_lines':extended_leads,'extended_pose_companion_lines':extended_companions})
result.update({'baked_page_events':baked['events'],'baked_backgrounds':len(baked['backgrounds']),
               'baked_scene_catalog_sha256':baked['catalog_sha256'],'baked_pages_have_no_physical_cast':True,
               'baked_native_payloads':baked_payloads})
result['all_portraits_used']=True # Authored cues and the explicit gallery cover every portrait above.
(a.output/'story-assets-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
