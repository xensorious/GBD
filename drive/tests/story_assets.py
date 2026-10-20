#!/usr/bin/env python3
"""Independently check cartridge archive offsets, art masks and resident budgets."""
import argparse,hashlib,json,struct
from pathlib import Path
from inspect_rom import inspect
from game_snapshot import find_symbol
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/story-release');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build/Coastline.nds';r=rom.read_bytes();fat,size=struct.unpack_from('<II',r,0x48);assert size==8
start,end=struct.unpack_from('<II',r,fat);archive=r[start:end];assert archive==(ROOT/'build/nitro/story.bin').read_bytes()
magic,event_table,events,asset_table,assets,total,unused=struct.unpack_from('<8s6I',archive)
assert magic==b'CSTORY02' and events==125 and assets==18 and total==len(archive)
lines=0;methods=set();speakers=set();backgrounds=set();longest=0
for event in range(events):
 offset,bytes_,count,reserved=struct.unpack_from('<IHHI',archive,event_table+event*12);n,pad=struct.unpack_from('<HH',archive,offset);assert count==n and 4<=n<=16
 assert offset+bytes_<=len(archive) and not pad and not reserved
 for line in range(n):
  ptr=struct.unpack_from('<I',archive,offset+4+line*4)[0];speaker,companion,method,bg,focus,length=struct.unpack_from('<BBBBHH',archive,ptr)
  assert speaker<14 and (companion<12 or companion==255) and method<4 and bg<6 and focus<=256 and 0<length<192
  text=archive[ptr+8:ptr+8+length].decode('ascii');assert '\0' not in text and len(text)==length
  assert not any(token in text for token in ('TODO','TBD','Proposed objective','IDEA:'))
  methods.add(method);speakers.add(speaker);backgrounds.add(bg);lines+=1;longest=max(longest,length)
assert lines==628 and methods==set(range(4)) and speakers==set(range(14)) and backgrounds==set(range(6))
art=[]
for i in range(assets):
 offset,w,h,size,kind=struct.unpack_from('<IHHII',archive,asset_table+i*16);assert size==w*h and offset+512+size<=len(archive)
 assert (w,h)==((512,216) if i<6 else (128,128))
 palette=struct.unpack_from('<256H',archive,offset);pixels=archive[offset+512:offset+512+size]
 if kind:assert palette[0]==0 and 100<pixels.count(0)<size-1000
 else:assert all(c&32768 for c in palette)
 art.append({'id':i,'width':w,'height':h,'transparent':bool(kind),'sha256':hashlib.sha256(pixels).hexdigest()})
elf=ROOT/'build/arm9.elf';main_end=find_symbol(elf,'__bss_end__');itcm_end=find_symbol(elf,'__itcm_end')
assert main_end<0x02380000 and itcm_end<=0x01008000
result={'rom':inspect(rom),'archive_matches_nitrofs':True,'archive_bytes':len(archive),'scenes':events,'lines':lines,'longest_line':longest,'methods':sorted(methods),'all_12_character_sprites':True,'portrait_alpha_masks_valid':True,'resident_end':hex(main_end),'resident_headroom_bytes':0x02380000-main_end,'itcm_headroom_bytes':0x01008000-itcm_end,'shared_scratch_bytes':256000,'art':art}
(a.output/'story-assets-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
