"""Target-backed code-only wrapper integration against current compiled ELF."""
import struct
from target import ROM, PARTS
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
rom=ROM;u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
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
def w32(a,v):u.mem_write(a,struct.pack('<I',v))
def r32(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def hook(uc,a,n,_):
 global saved_fast_lr
 if a==END:uc.emu_stop()
 elif a==0x0891cecc or a==0x0806e6d0:
  if a==0x0806e6d0:uc.reg_write(UC_ARM_REG_R0,0)
  uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 elif a==0x0203f200:
  assert r32(0x030030f4)==0x08011101
  calls.append(a);uc.reg_write(UC_ARM_REG_PC,saved_fast_lr)
 elif a in (0x08015910,0x08015c00,0x08015c74,0x080123e4,0x08011100,0x0200f000,FAKE):
  calls.append(a)
  if require_identity and a in (0x080123e4,0x08011100,0x0200f000):assert r32(0x030030f4)==0x08011101
  if a in (0x08015910,0x08015c00):w32(MAIN,0x08014041)
  if a==0x0200f000 and nested_fast:
   saved_fast_lr=uc.reg_read(UC_ARM_REG_LR)
   uc.reg_write(UC_ARM_REG_LR,0x0203f201);uc.reg_write(UC_ARM_REG_PC,r32(0x02010b20));return
  if mutate_cb2 is not None and a in (0x080123e4,0x08011100,0x0200f000):w32(0x030030f4,mutate_cb2)
  uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,hook)
def run(entry):
 u.reg_write(UC_ARM_REG_SP,0x03006f00);u.reg_write(UC_ARM_REG_LR,END|1)
 u.emu_start(entry|1,0,count=100000)
 assert u.reg_read(UC_ARM_REG_PC)==END and u.reg_read(UC_ARM_REG_SP)==0x03006f00
w32(S,0x414d5954);w32(S+4,0x52554e31);u.mem_write(0x04000130,b'\xff\x03')
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
u.mem_write(0x088ce782,struct.pack('<H',0x0201));w32(0x02023bc8,0);calls.clear()
h=u.hook_add(UC_HOOK_CODE,lambda uc,a,n,_:uc.emu_stop() if a==0x088ce53e else None)
u.reg_write(UC_ARM_REG_R2,10);u.reg_write(UC_ARM_REG_SP,0x03006f00);u.emu_start(0x088ce535,0,count=100000);u.hook_del(h)
assert u.reg_read(UC_ARM_REG_PC)==0x088ce53e and calls==[FAKE],calls
checks+=1
print('PASS',checks,'current compiled ReadKeys-installed dispatcher cases')

def hooks(enabled=True):
 u.mem_write(0x080005ec,struct.pack('<I',0x0200cecd if enabled else 0x0891cecd))
 u.mem_write(0x08002b84,struct.pack('<H',0x4710 if enabled else 0x6010))

newchecks=0
callback_guard=r32(0x02010b20)
callback_shim=r32(0x02010b24)
for original in [0x080155c9,0x088ce0cd]:
 hooks();w32(MAIN,original);w32(0x030030f0,0x080123e5);w32(0x030030f4,0x08011101)
 w32(S+92,0x43484541);run(0x0200cecd)
 assert r32(MAIN)==0x0200e001 and r32(0x030030f0)==callback_guard and r32(0x030030f4)==callback_shim
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
print('PASS',newchecks,'clone callback mapping and disabled-wrapper restoration cases')

identitychecks=0
require_identity=True
for changed in [None,0x080565b5]:
 hooks();w32(0x030030f4,callback_shim);calls.clear();mutate_cb2=changed
 run(callback_shim)
 assert calls==[0x0200f000]
 assert r32(0x030030f4)==(callback_shim if changed is None else changed)
 identitychecks+=1
mutate_cb2=None
hooks();w32(MAIN,0x08014041);w32(0x030030f4,callback_shim);calls.clear()
run(callback_guard)
assert calls==[0x080123e4] and r32(0x030030f4)==callback_shim
identitychecks+=1
hooks();w32(0x030030f4,callback_shim);calls.clear();nested_fast=True
run(callback_shim);nested_fast=False
assert calls==[0x0200f000,0x080123e4,0x0203f200] and r32(0x030030f4)==callback_shim
identitychecks+=1
hooks(False);w32(0x030030f4,callback_shim);calls.clear()
run(callback_shim)
assert calls==[0x08011100] and r32(0x030030f4)==0x08011101
identitychecks+=1
print('PASS',identitychecks,'callback2 public-identity preservation cases, including nested fast-loop guard')
