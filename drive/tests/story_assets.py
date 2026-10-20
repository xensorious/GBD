#!/usr/bin/env python3
"""Independently check cartridge archive offsets, art masks and resident budgets."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from inspect_rom import inspect
from game_snapshot import find_symbol
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from story_script import load_story,CAST,PEOPLE
source_backgrounds,source_scenes=load_story(ROOT)
manifest=json.loads((ROOT/'build/story-manifest.json').read_text());nbg=len(source_backgrounds);portrait_map=manifest['portrait_map'];portraits_seen=set();background_events={}
expected_lines=sum(len(e['lines']) for e in source_scenes)
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/story-release');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build/Coastline.nds';r=rom.read_bytes();fat,size=struct.unpack_from('<II',r,0x48);assert size==8
start,end=struct.unpack_from('<II',r,fat);archive=r[start:end];assert archive==(ROOT/'build/nitro/story.bin').read_bytes()
magic,event_table,events,asset_table,assets,total,unused=struct.unpack_from('<8s6I',archive)
assert magic==b'CSTORY02' and events==125 and assets==manifest['assets'] and total==len(archive)
lines=0;methods=set();speakers=set();backgrounds=set();longest=0
for event in range(events):
 offset,bytes_,count,reserved=struct.unpack_from('<IHHI',archive,event_table+event*12);n,pad=struct.unpack_from('<HH',archive,offset);assert count==n and 4<=n<=16
 assert offset+bytes_<=len(archive) and not pad and not reserved
 for line in range(n):
  ptr=struct.unpack_from('<I',archive,offset+4+line*4)[0];speaker,companion,method,bg,focus,length=struct.unpack_from('<BBBBHH',archive,ptr)
  assert speaker<len(CAST) and (companion<len(PEOPLE) or companion==255) and method<4 and bg<nbg and (focus&511)<=256 and not focus&0x2000 and 0<length<192
  text=archive[ptr+8:ptr+8+length].decode('ascii');assert '\0' not in text and len(text)==length
  authored=source_scenes[event]['lines'][line]
  expected_focus=authored['focus']|(0x8000 if authored['move'] else 0)|(0x4000 if authored['caption_right'] else 0)|(authored['pose']<<9)|(authored['companion_pose']<<11)
  assert (speaker,companion,method,bg,focus,text)==(authored['speaker'],authored['companion'],authored['stage'],authored['background'],expected_focus,authored['text'])
  assert not any(token in text for token in ('TODO','TBD','Proposed objective','IDEA:'))
  methods.add(method);speakers.add(speaker);backgrounds.add(bg);lines+=1;longest=max(longest,length)
  background_events.setdefault(bg,set()).add(event)
  if speaker<len(PEOPLE):portraits_seen.add(portrait_map[(focus>>9)&3][speaker])
  if method==0 and companion<len(PEOPLE):portraits_seen.add(portrait_map[(focus>>11)&3][companion])
assert lines==expected_lines and methods==set(range(4)) and speakers==set(range(len(CAST))) and backgrounds==set(range(nbg))
assert portraits_seen==set(range(manifest['portraits']))
shot_count=0
for scene in source_scenes:
 for shot in scene['shots']:
  shot_count+=1
  # Independent projection check: the named source subject must be inside the
  # final crop, including the wider letterboxed montage lens.
  width=276 if shot['stage']==3 else 256
  left=min(shot['focus'],512-width)
  assert left<=shot['subject_x']<left+width,(scene['id'],shot['subject'])
  if shot['stage']!=0:assert all(l['companion']==255 for l in shot['lines'])
assert shot_count==sum(len(e['shots']) for e in source_scenes)
important={'D01':{('BENCH','JACK'),('BENCH','KEYS')},'D04':{('NIA_KITCHEN','TABLE'),('PAPERS','LEASE')},
 'D10':{('MINA_BEDROOM','NOTES'),('MINA_BEDROOM','PHOTOS')},'D17':{('JULES_ROOM','HELMET'),('JULES_ROOM','BILLS')},
 'D19':{('CORNER','MARKER'),('CORNER','EXIT')},'D24':{('LIGHTHOUSE','PHOTO'),('LIGHTHOUSE','SEA')},
 'C07':{('RAIN','WATER'),('RAIN','AWNING')},'C10':{('HARBOR_BATHROOM','BASIN'),('HARBOR_BATHROOM','MIRROR')},
 'C19':{('WAITING_ROOM','CHAIRS'),('WAITING_ROOM','FLOWERS')},'B20':{('OLD_WORKSHOP','LIFT'),('OLD_WORKSHOP','CHAIR')},
 'A13':{('SKY_LOUNGE','FOLDER'),('SKY_LOUNGE','WINDOW')},'A21':{('OFFICE','CONTRACT'),('OFFICE','LICENSE')},
 'S20':{('OFFICE','CONTRACT'),('OFFICE','LICENSE')},'S25':{('NIGHTFINAL','CORNER'),('EPILOGUE','KEYS'),('EPILOGUE','CHAIRS')}}
for key,subjects in important.items():
 scene=next(s for s in source_scenes if s['id']==key)
 assert subjects<={(source_backgrounds[s['background']]['name'],s['subject']) for s in scene['shots']},key
art=[]
for i in range(assets):
 offset,w,h,size,kind=struct.unpack_from('<IHHII',archive,asset_table+i*16);assert size==w*h and offset+512+size<=len(archive)
 assert (w,h)==((512,216) if i<nbg else (128,128))
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
 art.append({'id':i,'width':w,'height':h,'transparent':bool(kind),'sha256':hashlib.sha256(pixels).hexdigest()})
elf=ROOT/'build/arm9.elf';main_end=find_symbol(elf,'__bss_end__');itcm_end=find_symbol(elf,'__itcm_end')
assert main_end<0x02380000 and itcm_end<=0x01008000
result={'rom':inspect(rom),'archive_matches_nitrofs':True,'archive_bytes':len(archive),'scenes':events,'lines':lines,'backgrounds':nbg,'people':len(PEOPLE),'portraits':manifest['portraits'],'all_portraits_used':True,'no_large_atlas_fragments':True,'one_event_new_backgrounds':sum(len(v)==1 for k,v in background_events.items() if k>=18),'new_place_events':len(set.union(*(v for k,v in background_events.items() if k>=18))),'authored_shots':shot_count,'compiled_directions_match_source':True,'subjects_in_final_frame':True,'shot_local_companions':True,'longest_line':longest,'methods':sorted(methods),'all_character_sprites':True,'portrait_alpha_masks_valid':True,'resident_end':hex(main_end),'resident_headroom_bytes':0x02380000-main_end,'itcm_headroom_bytes':0x01008000-itcm_end,'shared_scratch_bytes':256000,'art':art}
(a.output/'story-assets-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
