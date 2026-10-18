"""Play the real ROM through mGBA using buttons; never writes game RAM.

The ELF symbols are used only to observe state and route around obstacles.
Requires the mGBA development library and native gcc, in GBA_SYSROOT or system.
"""
from pathlib import Path
import ctypes as C
import subprocess
import os
from collections import deque
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
SYSROOT=Path(os.environ.get('GBA_SYSROOT','/workspace/gba-tools/sysroot'))
TOOL=os.environ.get('GBA_TOOLCHAIN','/workspace/gba-tools/sysroot/usr/bin/arm-none-eabi-')
A,B,SELECT,START,RIGHT,LEFT,UP,DOWN,R,L=1,2,4,8,16,32,64,128,256,512
TITLE,FIELD,DIALOG,CHOOSE,MENU,TEAM,GUIDE,JOURNAL,BATTLE,GROWTH,ENDING=range(11)

def build_bridge():
    cmd=['gcc','-O2','-shared','-fPIC',str(ROOT/'tests/emulator_bridge.c'),'-lmgba','-o',str(BUILD/'emulator_bridge.so')]
    if SYSROOT.exists():
        cmd[1:1]=['-I'+str(SYSROOT/'usr/include'),'-L'+str(SYSROOT/'usr/lib/x86_64-linux-gnu'),'-Wl,-rpath,'+str(SYSROOT/'usr/lib/x86_64-linux-gnu')]
    subprocess.run(cmd,check=True)

class Friend(C.Structure):
    _fields_=[(n,C.c_uint8) for n in ['species','level','path','stationed','hp','energy','xp','bond']]
class Save(C.Structure):
    _fields_=[('magic',C.c_uint32),('serial',C.c_uint32)]+[(n,C.c_uint8) for n in ['version','map','x','y','dir','count','lead','starter','treats','tonics','scout','job','rival','seen','pickups','mute']]+[('friends',Friend*6),('steps',C.c_uint32),('checksum',C.c_uint32)]
class Battle(C.Structure):
    _fields_=[('foes',Friend*2)]+[(n,C.c_int) for n in ['trainer','foe','count','active','menu','cursor','turn','calm','guard','encounter','shake','result','logCount','logPos']]+[('logs',(C.c_char*112)*12)]

class Emulator:
    def __init__(self,save=None):
        self.lib=C.CDLL(str(BUILD/'emulator_bridge.so'))
        self.lib.emu_open.argtypes=[C.c_char_p]
        self.lib.emu_load_save.argtypes=[C.c_char_p]
        self.lib.emu_save.argtypes=[C.c_char_p]
        self.lib.emu_pixels.restype=C.c_void_p
        self.lib.emu_read.restype=C.c_uint32
        out=subprocess.check_output([TOOL+'nm',str(BUILD/'bramblebound.elf')],text=True)
        self.sym={v[2]:int(v[0],16) for line in out.splitlines() if len(v:=line.split())==3}
        assert self.lib.emu_open(str(BUILD/'Bramblebound.gba').encode())
        if save is not None:assert self.lib.emu_load_save(str(save).encode())
        self.frames(100)
    def close(self):self.lib.emu_close()
    def frames(self,n,key=0):self.lib.emu_frame(key,n)
    def read(self,addr,n=1):return self.lib.emu_read(addr,n)
    def data(self,name,cls):
        addr=self.sym[name]
        return cls.from_buffer_copy(bytes(self.read(addr+i) for i in range(C.sizeof(cls))))
    @property
    def state(self):return self.data('g',Save)
    @property
    def fight(self):return self.data('battle',Battle)
    @property
    def mode(self):return self.read(self.sym['screen'],4)
    def tap(self,key):self.frames(10,key);self.frames(10)
    def screenshot(self,name):
        self.frames(8)
        im=Image.frombytes('RGBA',(240,160),C.string_at(self.lib.emu_pixels(),240*160*4)).convert('RGB')
        im.save(BUILD/(name+'.png'))
    def dismiss(self):
        for _ in range(50):
            if self.mode!=DIALOG:return
            self.tap(A)
        raise AssertionError('dialog did not finish')
    def logs(self):
        for _ in range(100):
            if self.mode!=BATTLE or self.fight.logPos>=self.fight.logCount:return
            self.tap(A)
        raise AssertionError('battle messages did not finish')
    def field(self):
        self.logs();self.dismiss()
        if self.mode==GROWTH:self.tap(A);self.dismiss()
        assert self.mode in (FIELD,ENDING),self.mode
    def move(self,key):
        old=(self.state.map,self.state.x,self.state.y)
        for _ in range(55):
            self.frames(2,key)
            if (self.state.map,self.state.x,self.state.y)!=old:
                self.frames(12);return
            if self.mode!=FIELD:break
        raise AssertionError(('blocked',old,key,self.mode))
    def walk(self,x,y):
        assert self.mode==FIELD
        start=(self.state.x,self.state.y);mapid=self.state.map
        world=[self.read(self.sym['world']+mapid*768+i) for i in range(768)]
        npcs={(15,10),(6,11),(24,14),(11,16)} if mapid==0 else {(4,19),(7,12),(26,5)}
        if mapid:
            for i in range(6):
                addr=self.sym['encounters']+i*5
                if not self.read(addr+4):npcs.add((self.read(addr),self.read(addr+1)))
        frontier=deque([start]);paths={start:[]}
        while frontier:
            at=frontier.popleft()
            if at==(x,y):break
            for dx,dy,k in [(0,-1,UP),(0,1,DOWN),(-1,0,LEFT),(1,0,RIGHT)]:
                nx,ny=at[0]+dx,at[1]+dy;p=(nx,ny)
                if not (0<=nx<32 and 0<=ny<24) or p in paths or p in npcs:continue
                if world[ny*32+nx] in (2,3,8,10,11,12,13,15):continue
                paths[p]=paths[at]+[k];frontier.append(p)
        assert (x,y) in paths,('no route',start,(x,y))
        for key in paths[(x,y)]:self.move(key)
        assert (self.state.x,self.state.y)==(x,y)
    def trail(self):
        self.walk(15,1);self.move(UP);assert self.state.map==1
    def town(self):
        self.walk(4,22);self.move(DOWN);assert self.state.map==0
    def pick(self,selection,menu=None):
        if menu is not None:
            assert self.fight.menu==0
            if menu&1:self.tap(RIGHT)
            if menu&2:self.tap(DOWN)
            self.tap(A)
        current=self.fight.cursor
        if (selection&1)!=(current&1):self.tap(RIGHT)
        if (selection&2)!=(current&2):self.tap(DOWN)
        self.tap(A)

def assert_header():
    b=(BUILD/'Bramblebound.gba').read_bytes()
    assert b[160:172]==b'BRAMBLEBOUND' and b[178]==0x96
    assert (sum(b[160:190])+0x19)&255==0
    assert b'SRAM_V113' in b
    assert len(b)<=32*1024*1024

def start(e,starter):
    assert e.mode==TITLE
    e.tap(A);e.dismiss();assert e.mode==FIELD
    e.walk(15,11);e.tap(A);e.dismiss();assert e.mode==CHOOSE
    for _ in range(starter):e.tap(RIGHT)
    e.screenshot('starter-selection' if starter==0 else f'starter-{starter}')
    e.tap(A);e.dismiss();assert e.state.count==1 and e.state.friends[0].species==starter

def recruit_first(e):
    e.trail();e.walk(4,20);e.tap(A);e.dismiss();assert e.state.scout
    e.walk(7,18);e.tap(A);e.logs();assert e.mode==BATTLE
    before=e.state.treats;e.pick(1);e.logs()
    assert e.state.treats==before and not e.fight.result,'premature recruit consumed a treat'
    e.pick(3,menu=0);e.logs() # Soothe instead of damaging a frightened creature.
    assert e.fight.calm
    e.pick(1);e.logs();e.dismiss();assert e.mode==GROWTH
    e.screenshot('growth-choice')
    e.tap(A);e.dismiss()
    assert e.state.count==2 and e.state.friends[0].path==1
    assert e.state.treats==before-1

def garden(e):
    e.town();e.walk(24,13);e.tap(A);e.dismiss()
    assert e.state.job and e.state.friends[1].stationed
    e.screenshot('garden-helper')
    e.tap(A);e.dismiss();assert not e.state.friends[1].stationed

def camp_and_revision(e):
    e.walk(7,13);e.tap(A);e.dismiss()
    before=e.state.friends[0].level
    assert all(f.hp>0 for f in list(e.state.friends)[:e.state.count])
    e.tap(START);e.tap(A);assert e.mode==TEAM
    e.tap(SELECT);assert e.mode==GROWTH
    e.tap(RIGHT);e.tap(A);e.dismiss()
    assert e.state.friends[0].path==2 and e.state.friends[0].level==before

def rival(e,use_burst=False):
    e.walk(26,6);e.tap(A);e.dismiss();e.logs();assert e.mode==BATTLE and e.fight.trainer
    e.screenshot('rival-battle')
    turns=0
    while e.mode==BATTLE and turns<60:
        e.logs()
        if e.mode!=BATTLE:break
        b=e.fight;p=e.state.friends[b.active]
        assert b.menu==0,(b.menu,b.cursor)
        # Use only buttons and public battle information: energy and HP.
        if p.hp<13 and e.state.tonics:
            e.pick(0,menu=3)
        else:
            move=3 if use_burst and p.path==1 and p.energy>=3 else 1 if p.energy>=2 else 2
            e.pick(move,menu=0)
        e.logs();turns+=1
    assert e.mode==ENDING,('rival not won',e.mode,turns,e.state.rival)
    assert e.state.rival
    e.screenshot('first-trail-pin')
    e.tap(A);assert e.mode==FIELD
    print('Rival won in',turns,'player actions')

def verify_save(e):
    saved=BUILD/'test-battery.sav'
    before=e.state
    assert e.lib.emu_save(str(saved).encode())
    e.close()
    e=Emulator(saved)
    assert e.mode==TITLE
    e.tap(DOWN);e.tap(A);assert e.mode==11
    e.tap(A);assert e.mode==TITLE # Cancel is the safe default for NEW JOURNEY.
    e.tap(UP)
    e.tap(A)
    assert e.mode==FIELD and e.state.rival==before.rival and e.state.count==before.count
    assert e.state.friends[0].path==before.friends[0].path
    print('Battery save restored after a fresh emulator boot')
    e.close()
    damaged=bytearray(saved.read_bytes());damaged[0]^=0xff
    fallback=BUILD/'test-backup.sav';fallback.write_bytes(damaged)
    e=Emulator(fallback);e.tap(A)
    assert e.mode==FIELD and e.state.rival==before.rival and e.state.count==before.count
    e.close()
    print('Corrupted primary save recovered from validated backup')

def lose_and_recover(e):
    e.walk(26,6);before=e.state
    e.tap(A);e.dismiss();e.logs();assert e.mode==BATTLE
    for _ in range(120):
        if e.mode!=BATTLE:break
        e.pick(2,menu=0);e.logs() # Deliberately rest forever instead of fighting.
    assert e.mode==DIALOG,'expected safe loss recovery'
    e.dismiss();after=e.state
    assert e.mode==FIELD and after.map==0 and (after.x,after.y)==(6,12)
    assert not after.rival and after.treats==before.treats and after.tonics==before.tonics
    assert all(f.hp for f in list(after.friends)[:after.count])
    print('Intentional loss returns a healed team without inventory penalties')
    e.trail()

def extra_checks(e):
    e.tap(START);e.tap(DOWN);e.tap(A);assert e.mode==GUIDE
    e.tap(R);e.screenshot('field-guide');e.tap(B);e.tap(DOWN);e.tap(A)
    assert e.mode==JOURNAL;e.screenshot('trail-journal');e.tap(B);e.tap(B)
    assert e.mode==FIELD
    e.walk(10,15);e.tap(A);e.logs();assert e.mode==BATTLE
    before=e.state.treats
    for _ in range(10):
        b=e.fight;foe=b.foes[b.foe]
        maxhp=[25,23,27,29,23,25][foe.species]+(foe.level-1)*3
        if foe.hp<=maxhp//2:break
        e.pick(0,menu=0);e.logs()
    assert e.mode==BATTLE and not e.fight.calm
    e.pick(1);e.field()
    assert e.state.count==3 and e.state.treats==before-1
    assert any(f.species==5 for f in list(e.state.friends)[:e.state.count])
    e.walk(7,18);e.tap(A);e.logs();assert e.mode==BATTLE
    before=e.state.treats;e.pick(1);e.logs()
    assert e.state.count==3 and e.state.treats==before and not e.fight.result
    e.tap(B);e.field()
    assert e.state.treats==before
    print('Half-health recruitment, duplicate protection, retreat, guide, and journal pass')

def main():
    os.chdir(ROOT);build_bridge();assert_header()
    for starter in range(3):
        e=Emulator();e.screenshot('title');start(e,starter);recruit_first(e)
        if starter==0:garden(e);e.trail()
        camp_and_revision(e)
        if starter==0:lose_and_recover(e)
        rival(e)
        if starter==0:extra_checks(e)
        assert e.lib.emu_audio_nonzero()>0,'PSG output was silent'
        verify_save(e)
        print('PASS starter',starter)
    for starter in range(3):
        e=Emulator();start(e,starter);recruit_first(e)
        assert e.state.friends[0].path==1 and e.state.tonics==3 and not e.state.job
        rival(e,use_burst=True);e.close()
        print('PASS minimum STRIDER route',starter)
    e=Emulator();start(e,0);recruit_first(e);camp_and_revision(e)
    assert e.state.tonics==3 and not e.state.job
    rival(e);e.close()
    print('PASS minimum WARDEN Mossprig route without optional supplies')
    print('PASS: all starters, recruitment, growth revision, garden, rival, and battery save.')

if __name__=='__main__':main()
