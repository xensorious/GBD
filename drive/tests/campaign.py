#!/usr/bin/env python3
"""Story cartridge tour: joypad input and read-only RAM only.
Later-chapter visual fixtures are explicitly labelled; earned progression is
verified separately through the first six episodes and fresh profile reloads.
"""
import argparse,ctypes as C,hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
from emulator import Emulator,ROOT
from game_snapshot import Game,Save,find_symbol,symbol_size,enter_opening
from playthrough import DriveThrough
from fixtures import encode_profile,write_battery,assert_compact_battery
class Line(C.LittleEndianStructure):
 _fields_=[(n,C.c_uint8) for n in ('speaker','companion','stage','background')]+[('focus',C.c_uint16),('length',C.c_uint8),('pose_flags',C.c_uint8),('text',C.c_char*192)]
class Story(C.LittleEndianStructure):
 _fields_=[('offsets',C.c_uint32*16),('revision',C.c_uint32),('current',Line),('previous',Line)]+[(n,C.c_uint16) for n in ('revealed','ticks','age')]+[(n,C.c_uint8) for n in ('active','chapter','event','line','count','phase','phase_tick','replay','pause','cursor','failed')]
class Campaign(C.LittleEndianStructure):
 _fields_=[(n,C.c_uint8) for n in ('tab','chapter','event','modal','page','garage_return','drive_column')]+[('revision',C.c_uint32)]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/story-release');p.add_argument('--native-only',action='store_true');p.add_argument('--emulator-only',action='store_true');p.add_argument('--verify-progress',type=int);p.add_argument('--slot',type=int,default=0);p.add_argument('--save-dir',type=Path);p.add_argument('--case');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build/Coastline.nds';elf=ROOT/'build/arm9.elf';addr=find_symbol(elf,'game');sa=find_symbol(elf,'story');ca=find_symbol(elf,'campaign');va=find_symbol(elf,'story_visual_stats')
assert symbol_size(elf,'story')==C.sizeof(Story)==488 and symbol_size(elf,'campaign')==C.sizeof(Campaign)==12
sha=hashlib.sha256(rom.read_bytes()).hexdigest()
manifest=json.loads((ROOT/'build/story-manifest.json').read_text());line_count=manifest['dialogue_lines']
def view(e):return Story.from_buffer_copy(e.read_ram(sa,C.sizeof(Story)))
def menu_state(e):return Campaign.from_buffer_copy(e.read_ram(ca,C.sizeof(Campaign)))
def enter_career(d):
 d.menu(0);d.press('a');assert d.game().mode==11;d.press('down');d.press('a');assert d.game().mode==12

def play(d,e,label,expected,shots):
 captured=set();start=d.game().frame;hardware=e.frames
 for step in range(1200):
  s=view(e);assert d.game().mode==13 and not s.failed
  if s.phase==4:
   assert d.game().save.reserved[0]==expected
   d.ticks(16)
   if not s.replay:assert d.game().save_ok and not d.game().save_requested,('Scene autosave failed',label)
   if label in ('D01','D06','S25'):d.shot('story-'+label.lower()+'-clear')
   d.press('a');assert d.game().mode==12
   assert not any((C.c_uint32*5).from_buffer_copy(e.read_ram(va,20))[:1])
   return {'scene':label,'dialogue_lines':s.count,'updates':d.game().frame-start,'lcd_frames':e.frames-hardware}
  if s.phase!=1:d.ticks(2);continue
  if s.revealed<s.current.length:d.press('a');continue
  if s.line not in captured and (s.line in shots or (s.current.stage==3 and label in ('D06','B02'))):
   d.ticks(40);d.shot('story-'+label.lower()+'-'+str(s.line));captured.add(s.line)
  d.press('a')
 raise AssertionError(('Story did not finish',label,view(e).phase))

if a.verify_progress is not None:
 with Emulator(rom,a.save_dir) as e:
  e.run(180);enter_opening(e,elf,a.slot);d=DriveThrough(e,addr,out,enter_menu=False)
  assert d.game().save.version==7 and d.game().save.reserved[0]==a.verify_progress
  if a.slot==0:
   actual=d.game().save;actual.checksum=0;assert bytes(actual)==(out/'story-earned-expected.bin').read_bytes()
  enter_career(d);s=menu_state(e);assert s.chapter==a.verify_progress//25 and s.event==a.verify_progress%25
  d.shot('story-profile-'+str(a.slot+1)+'-reload')
 print('Fresh profile reload:',a.slot+1,a.verify_progress,flush=True);raise SystemExit(0)

if a.case:
 ch='DCBAS'.index(a.case[0]);ev=int(a.case[1:])-1;directory=a.save_dir
 with Emulator(rom,directory) as e:
  e.run(180);d=DriveThrough(e,addr,out);enter_career(d);assert d.game().save.reserved[0]==ch*25+ev
  d.press('down');d.press('a');assert d.game().mode==13
  scene=manifest['events'][ch*25+ev];shots=set();at=0
  for shot in scene['shots']:
   shots.add(at)
   for n,line in enumerate(shot['lines']):
    if line['pose']>=3 and (not n or shot['lines'][n-1]['pose']!=line['pose']):shots.add(at+n)
    if line['companion_pose']>=4 and (not n or shot['lines'][n-1]['companion_pose']!=line['companion_pose']):shots.add(at+n)
   at+=len(shot['lines'])
  play(d,e,a.case,ch*25+ev+1,shots);d.shot('story-'+a.case.lower()+'-map')
 print('Later-chapter visual fixture reviewed:',a.case,flush=True);raise SystemExit(0)

if not a.emulator_only:
 cmd=['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-Isrc','-Ibuild','tests/campaign.c','src/game.c','src/story.c','src/story_io.c','src/road.c','src/traffic.c','src/gfx.c','build/assets.c','src/ui.c','src/render.c','src/story_render.c','src/dialogue_render.c','src/opening.c','-o','build/test-campaign']
 subprocess.run(cmd,cwd=ROOT,check=True);run=subprocess.run([str(ROOT/'build/test-campaign')],cwd=ROOT,check=True,capture_output=True,text=True);print(run.stdout,end='',flush=True)
 (out/'campaign-native-results.json').write_text(json.dumps({'sha256':sha,'checks':int(re.search(r'(\d+) checks',run.stdout)[1]),'sanitizers_passed':True,'all_scenes':125,'dialogue_lines':line_count,'result':run.stdout.strip()},indent=2)+'\n')
if a.native_only:raise SystemExit(0)

save=out/'story-earned-save';shutil.rmtree(save,ignore_errors=True);earned=[]
with Emulator(rom,save) as e:
 e.run(180);d=DriveThrough(e,addr,out);assert not d.game().save.reserved[0];original=d.game().save
 enter_career(d);d.shot('story-route-start');d.press('down');d.press('right');d.press('a');assert d.game().mode==12 and not d.game().save.reserved[0]
 d.press('left');d.press('a');assert d.game().mode==13
 d.ticks(24);before=view(e).line;d.ticks(4,touch=(120,80));assert view(e).line==before
 d.press('start');assert view(e).pause;d.shot('story-pause');d.press('down');d.press('down');d.press('a');assert d.game().mode==12 and not d.game().save.reserved[0]
 for index in range(6):
  assert menu_state(e).event==index;d.press('a');assert d.game().mode==13
  earned.append(play(d,e,f'D{index+1:02d}',index+1,{0,6} if index==0 else {0,3,6} if index==5 else {0} if index==4 else set()))
 d.shot('story-six-events-earned')
 d.press('down');d.press('down');d.press('a');assert d.game().mode==3 and menu_state(e).garage_return
 d.shot('story-shared-garage');d.press('r');d.press('b');assert d.game().mode==12 and d.game().save.reserved[0]==6
 d.press('down')
 for _ in range(6):d.press('left')
 d.press('a');assert view(e).replay;play(d,e,'D01-replay',6,set())
 assert d.game().save.cash==original.cash and d.game().save.owned==original.owned and d.game().save.total_runs==original.total_runs
 # Releasing the shared artwork store must restore both existing driving renderers.
 d.press('b');assert d.game().mode==11;d.press('up');d.press('a');assert d.game().mode==1;d.ticks(35,('r',));d.shot('story-return-highway',('r',))
 d.press('start');d.menu(1);d.press('a');assert d.game().mode==2;d.press('a')
 d.menu(0);d.press('a');d.press('right');d.press('a');assert int.from_bytes(e.read_ram(find_symbol(elf,'city'),4),'little')==1
 d.ticks(25,('r',));d.shot('story-return-city',('r',));mesh=list((C.c_uint32*6).from_buffer_copy(e.read_ram(find_symbol(elf,'city_mesh_last_stats'),24)));assert not mesh[4] and 20<mesh[0]<900
 d.press('start');d.menu(1);d.press('a');d.press('a');assert d.game().save.reserved[0]==6
 expected=d.game().save;expected.checksum=0;(out/'story-earned-expected.bin').write_bytes(bytes(expected))
for slot,progress in ((0,6),(1,0),(2,0),(0,6)):
 subprocess.run([sys.executable,__file__,'--output',str(out),'--save-dir',str(save),'--verify-progress',str(progress),'--slot',str(slot)],cwd=ROOT,check=True)
compact=assert_compact_battery(save/'Coastline.dsv')
# Explicitly labelled fixtures give visual coverage of later meetings and every cast member.
libpath=out/'libstory-qa.so';subprocess.run(['gcc','-std=c11','-O2','-shared','-fPIC','-Isrc','-Ibuild','src/game.c','src/story.c','src/story_io.c','src/road.c','src/traffic.c','-o',str(libpath)],cwd=ROOT,check=True)
lib=C.CDLL(str(libpath));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
from concurrent.futures import ThreadPoolExecutor
import threading
fixture_lock=threading.Lock()
cases=['D09','D10','D12','D17','D19','D24','C07','C10','C11','C12','C19','B02','B07','B15','B19','B20','B25','A06','A10','A13','A14','A21','A25','S04','S09','S12','S20','S23','S25']
def later(case):
 with fixture_lock:
  profile=Game();lib.game_init(C.byref(profile),123);profile.save.reserved[0]='DCBAS'.index(case[0])*25+int(case[1:])-1
  directory=out/('story-visual-'+case);write_battery(save/'Coastline.dsv',directory/'Coastline.dsv',encode_profile(lib,profile))
 with (out/('story-visual-'+case+'.log')).open('w') as log:
  subprocess.run([sys.executable,__file__,'--output',str(out),'--save-dir',str(directory),'--case',case],cwd=ROOT,check=True,stdout=log,stderr=subprocess.STDOUT)
with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(later,cases))
assert hashlib.sha256(rom.read_bytes()).hexdigest()==sha
result={'sha256':sha,'chapters':5,'events':125,'dialogue_lines':line_count,'backgrounds':manifest['backgrounds'],'portrait_variants':manifest['portraits'],'tabs':['Route','Story','Rivals','Plans','Office'],'button_only':True,'touch_ignored':True,'read_only_emulated_ram':True,'earned_story_events':earned,'saved_progress':6,'scene_autosaves_verified':True,'fresh_process_reloads':4,'three_profiles_isolated':True,'replay_does_not_advance':True,'abort_does_not_advance':True,'locked_events_blocked':True,'no_racing_cash_awarded':True,'shared_garage_return':True,'existing_endless_launch':True,'existing_city_launch':True,'staging':['portrait','panorama','radio','montage'],'later_visual_fixtures':cases,'fixture_note':'Later chapter batteries are explicitly manufactured visual-review fixtures, not evidence of emulator-earned progression. Native tests complete all 125 scenes through actual game logic. First six events are earned through joypad input on the ROM.','race_gameplay':False,'save_version':7,**compact}
(out/'campaign-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
