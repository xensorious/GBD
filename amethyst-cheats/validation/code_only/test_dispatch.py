"""Target-backed code-only wrapper integration against current compiled ELF."""
import struct
from target import ROM, PARTS
from tools.code_only_relocate import CB2_DEST
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
rom=ROM;u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
SECTIONS={name:(address,data) for name,address,data in PARTS}
TEXT_START,TEXT_DATA=SECTIONS['.text']
TEXT_END=TEXT_START+len(TEXT_DATA)
CALLBACK_EXPORT=SECTIONS['.callbacks'][0]
for a,n in [(0x08000000,0x2000000),(0x02000000,0x40000),(0x03000000,0x8000),(0x04000000,0x1000)]:u.mem_map(a,n)
u.mem_write(0x08000000,rom)
u.mem_write(0x080005ec,struct.pack('<I',0x0200cecd));u.mem_write(0x08002b84,struct.pack('<H',0x4710));u.mem_write(0x08002b96,struct.pack('<H',0x0201))
for n,a,b in PARTS:u.mem_write(a,b)
S=0x02010c00;END=0x03007c00;MAIN=0x03004f84;FAKE=0x0203f000;SCRIPT=0x0203f100
calls=[]
require_identity=False
mutate_cb2=None
nested_fast=False
saved_fast_lr=0
master_requests=[]
callback_requests=[]
ACTION=0x4143544e
BATTLE=0x4241544c
def w32(a,v):u.mem_write(a,struct.pack('<I',v))
def r32(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def apply_request(request):
 # Model the single reversible AR patch slot: its previous owner is restored
 # before the current selection/action-table patch is applied.
 u.mem_write(0x08014c1e,rom[0x14c1e:0x14c20])
 u.mem_write(0x088ce782,rom[0x8ce782:0x8ce784])
 if request==BATTLE:u.mem_write(0x08014c1e,struct.pack('<H',0x0200))
 elif request==ACTION:u.mem_write(0x088ce782,struct.pack('<H',0x0201))
 w32(S+92,0x43484541)
def selection_live():return r32(0x08014c1c)==0x0200b960
def actions_live():return r32(0x088ce780)==0x02010038
def hook(uc,a,n,_):
 global saved_fast_lr
 if a==END:uc.emu_stop()
 elif a==0x08002b80:
  assert uc.reg_read(UC_ARM_REG_R0)==0 and uc.reg_read(UC_ARM_REG_R1)==0
  request=r32(S+108)
  master_requests.append(request);apply_request(request)
  uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 elif a==0x0891cecc or a==0x0806e6d0:
  if a==0x0806e6d0:uc.reg_write(UC_ARM_REG_R0,0)
  uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 elif a==0x0203f200:
  assert r32(0x030030f4)==0x08011101
  calls.append(a);uc.reg_write(UC_ARM_REG_PC,saved_fast_lr)
 elif a in (0x08015910,0x08015c00,0x08015c74,0x080155c8,0x080123e4,0x08011100,CB2_DEST,FAKE):
  calls.append(a)
  if require_identity and a in (0x080123e4,0x08011100,CB2_DEST):assert r32(0x030030f4)==0x08011101
  if a==0x080123e4:callback_requests.append((r32(S+108),selection_live(),actions_live()))
  if a in (0x08015910,0x08015c00):w32(MAIN,0x08014041)
  if a==CB2_DEST and nested_fast:
   saved_fast_lr=uc.reg_read(UC_ARM_REG_LR)
   uc.reg_write(UC_ARM_REG_LR,0x0203f201);uc.reg_write(UC_ARM_REG_PC,r32(CALLBACK_EXPORT));return
  if mutate_cb2 is not None and a in (0x080123e4,0x08011100,CB2_DEST):w32(0x030030f4,mutate_cb2)
  uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,hook)
def run(entry):
 u.reg_write(UC_ARM_REG_SP,0x03006f00);u.reg_write(UC_ARM_REG_LR,END|1)
 u.emu_start(entry|1,0,count=100000)
 assert u.reg_read(UC_ARM_REG_PC)==END and u.reg_read(UC_ARM_REG_SP)==0x03006f00
w32(S,0x414d5954);w32(S+4,0x52554e31);u.mem_write(0x04000130,b'\xff\x03')
u.mem_write(0x03003529,b'\x02')
w32(0x02023d74,SCRIPT);u.mem_write(SCRIPT,b'\x40');w32(0x0200b960+0x40*4,FAKE|1)
w32(S+92,0x43484541);run(0x0200cecd)
source=struct.unpack_from('<14I',rom,0x250038);shadow=struct.unpack('<14I',u.mem_read(0x02010038,56))
assert all(shadow[i]==source[i] for i in range(14) if i!=10) and shadow[10]!=source[10]
checks=1
for original in [0x08015911,0x08015c01,0x08015c75]:
 for action in [0,10,11,12,13]:
  for busy in [0,1]:
   w32(MAIN,original);u.mem_write(0x02023be3,bytes([action]));w32(0x02023bc8,busy)
   w32(S+92,0x43484541);run(0x0200cecd);wrapper=r32(MAIN);assert wrapper!=original
   w32(S+92,0x43484541);run(0x0200cecd);assert r32(MAIN)==wrapper
   calls.clear();run(wrapper)
   finish=original!=0x08015c75 and action in [11,12]
   expected=[original&~1] if finish else ([FAKE] if not busy else [])
   assert calls==expected,(hex(original),action,busy,calls,expected)
   assert r32(MAIN)==(0x08014041 if finish else wrapper)
   checks+=1
w32(S+108,ACTION);apply_request(ACTION);w32(0x02023bc8,0);calls.clear()
h=u.hook_add(UC_HOOK_CODE,lambda uc,a,n,_:uc.emu_stop() if a==0x088ce53e else None)
u.reg_write(UC_ARM_REG_R2,10);u.reg_write(UC_ARM_REG_SP,0x03006f00);u.emu_start(0x088ce535,0,count=100000);u.hook_del(h)
assert u.reg_read(UC_ARM_REG_PC)==0x088ce53e and calls==[FAKE],calls
checks+=1
print('PASS',checks,'current compiled ReadKeys-installed dispatcher cases')

def hooks(enabled=True):
 u.mem_write(0x080005ec,struct.pack('<I',0x0200cecd if enabled else 0x0891cecd))
 u.mem_write(0x08002b84,struct.pack('<H',0x4710 if enabled else 0x6010))

newchecks=0
callback_guard=r32(CALLBACK_EXPORT)
callback_shim=r32(CALLBACK_EXPORT+4)
execute_wrapper=None
for original in [0x080155c9,0x088ce0cd]:
 hooks();w32(MAIN,original);w32(0x030030f0,0x080123e5);w32(0x030030f4,0x08011101)
 w32(S+92,0x43484541);run(0x0200cecd)
 wrapper=r32(MAIN)
 assert wrapper&1 and TEXT_START<=(wrapper&~1)<TEXT_END
 if execute_wrapper is None:execute_wrapper=wrapper
 else:assert execute_wrapper==wrapper
 assert r32(0x030030f0)==callback_guard and r32(0x030030f4)==callback_shim
 assert actions_live() and not selection_live()
 newchecks+=1
for original in [0x08015911,0x08015c01,0x08015c75]:
 hooks();w32(MAIN,original);u.mem_write(0x02023be3,b'\0');w32(0x02023bc8,0)
 w32(S+92,0x43484541);run(0x0200cecd);wrapper=r32(MAIN)
 hooks(False);calls.clear();run(wrapper)
 assert calls==[original&~1],(hex(original),calls)
 assert r32(MAIN)==(0x08014041 if original in [0x08015911,0x08015c01] else original)
 newchecks+=1
for original in [0x08015911,0x08015c01,0x08015c75,0x080155c9]:
 hooks();w32(MAIN,original);w32(0x030030f0,0x080123e5);w32(0x030030f4,0x08011101)
 w32(S+92,0x43484541);run(0x0200cecd)
 hooks(False);calls.clear();run(callback_guard)
 assert calls==[0x080123e4] and r32(MAIN)==original
 assert r32(0x030030f0)==0x080123e5 and r32(0x030030f4)==0x08011101
 newchecks+=1
print('PASS',newchecks,'ROM action callback mapping and disabled-wrapper restoration cases')

# Every fast-loop CB1 must refresh the patch BEFORE invoking the original CB1,
# including phase changes that happen without another ReadKeys call.
phasechecks=0
hooks();w32(0x030030f4,callback_shim)
for main,request in [(0x080155c9,ACTION),(execute_wrapper,ACTION),
                     (0x08014041,BATTLE),(0x08015c75,BATTLE),
                     (0x088ce0cd,ACTION),(0x08014041,BATTLE)]:
 w32(MAIN,main);master_requests.clear();callback_requests.clear()
 previous=r32(S+108);calls.clear();run(callback_guard)
 assert master_requests==([] if previous==request else [request])
 assert callback_requests==[(request,request==BATTLE,request==ACTION)]
 assert calls==[0x080123e4]
 phasechecks+=1

# A direct call of the small ExecuteActions wrapper also repairs a stale
# selection-table owner before chaining the original, unrelocated routine.
w32(MAIN,execute_wrapper);w32(S+108,BATTLE);apply_request(BATTLE)
master_requests.clear();calls.clear();run(execute_wrapper)
assert master_requests==[ACTION] and calls==[0x080155c8]
assert actions_live() and not selection_live()
phasechecks+=1

# Selection-script dispatch remains protected when its phase regains the slot.
w32(MAIN,0x08014041);run(callback_guard);assert selection_live() and not actions_live()
calls.clear()
h=u.hook_add(UC_HOOK_CODE,lambda uc,a,n,_:uc.emu_stop() if a==0x08014bfa else None)
u.reg_write(UC_ARM_REG_R4,SCRIPT);u.reg_write(UC_ARM_REG_SP,0x03006f00)
u.emu_start(0x08014bed,0,count=100000);u.hook_del(h)
assert u.reg_read(UC_ARM_REG_PC)==0x08014bfa and calls==[FAKE],calls
phasechecks+=1
print('PASS',phasechecks,'shared patch-slot owner/phase and exact-ROM selection-dispatch cases')

identitychecks=0
require_identity=True
for changed in [None,0x080565b5]:
 hooks();w32(0x030030f4,callback_shim);calls.clear();mutate_cb2=changed
 run(callback_shim)
 assert calls==[CB2_DEST]
 assert r32(0x030030f4)==(callback_shim if changed is None else changed)
 identitychecks+=1
mutate_cb2=None
hooks();w32(MAIN,0x08014041);w32(0x030030f4,callback_shim);calls.clear()
run(callback_guard)
assert calls==[0x080123e4] and r32(0x030030f4)==callback_shim
identitychecks+=1
hooks();w32(0x030030f4,callback_shim);calls.clear();nested_fast=True
run(callback_shim);nested_fast=False
assert calls==[CB2_DEST,0x080123e4,0x0203f200] and r32(0x030030f4)==callback_shim
identitychecks+=1
hooks(False);w32(0x030030f4,callback_shim);calls.clear()
run(callback_shim)
assert calls==[0x08011100] and r32(0x030030f4)==0x08011101
identitychecks+=1
print('PASS',identitychecks,'callback2 public-identity preservation cases, including nested fast-loop guard')
