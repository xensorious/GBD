"""ROM-backed native entry/heap tests using current compiled ELF."""
import struct
from target import ROM, PAYLOAD
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
PARTS=PAYLOAD
S=0x0201be00;END=0x03007c00
CALLEE=[UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]
def setup(payload=True):
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
 for a,n in [(0x08000000,0x02000000),(0x02000000,0x40000),(0x03000000,0x8000),(0x04000000,0x1000)]:u.mem_map(a,n)
 u.mem_write(0x08000000,ROM)
 if payload:
  for n,a,b in PARTS:u.mem_write(a,b)
 return u
def w32(u,a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def r32(u,a):return struct.unpack('<I',u.mem_read(a,4))[0]
def launch(u,entry,args=(),stubs=None):
 events=[];stubs=stubs or {}
 u.reg_write(UC_ARM_REG_SP,0x03006f00);u.reg_write(UC_ARM_REG_LR,END|1)
 for i,r in enumerate(CALLEE):u.reg_write(r,0x45670000+i*4)
 for r,v in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3],args):u.reg_write(r,v)
 def hook(u,a,n,x):
  if a==END:u.emu_stop()
  elif a in stubs:
   events.append((a,tuple(u.reg_read(r) for r in [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3])))
   if stubs[a] is not None:u.reg_write(UC_ARM_REG_R0,stubs[a])
   u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 h=u.hook_add(UC_HOOK_CODE,hook);u.emu_start(entry|1,0,count=100000);u.hook_del(h)
 assert u.reg_read(UC_ARM_REG_PC)==END,hex(u.reg_read(UC_ARM_REG_PC))
 assert u.reg_read(UC_ARM_REG_SP)==0x03006f00
 assert [u.reg_read(r) for r in CALLEE]==[0x45670000+i*4 for i in range(8)]
 return u.reg_read(UC_ARM_REG_R0),events
checks=0
for start,size,warm in [(0x02000000,0x1c000,False),(0x02000000,0x1c000,True),(0x02008000,0x1000,True)]:
 u=setup();old=bytes([0xa5])*512;u.mem_write(S,old)
 if warm:w32(u,S,0x414d5954);old=bytes(u.mem_read(S,512))
 w32(u,0x03001f70,0x12345678);w32(u,0x03001f74,0x87654321)
 launch(u,0x09ff1011,[start,size]);adjusted=size-512 if start==0x02000000 and size==0x1c000 else size
 assert r32(u,0x03000a38)==start,hex(r32(u,0x03000a38))
 assert r32(u,0x03000a3c)==adjusted,hex(r32(u,0x03000a3c))
 assert r32(u,0x03001f70)==0x12345678 and r32(u,0x03001f74)==0x87654321
 ref=setup(False);launch(ref,0x08002b81,[start,adjusted])
 assert u.mem_read(start,16)==ref.mem_read(start,16)
 assert u.mem_read(S,512)==(old if warm else struct.pack('<I',0x414d5954)+bytes(508))
 checks+=1
for requested in [0x1bdf0,0x1bdf1,0x1bdf4,0x1bde0,0x1bdc0]:
 u=setup();launch(u,0x09ff1011,[0x02000000,0x1c000]);before=bytes(u.mem_read(S,512))
 pointer,events=launch(u,0x08002b9d,[requested],{0x081e3b14:None})
 assert pointer==(0x02000010 if requested<=0x1bdf0 else 0),(hex(requested),hex(pointer))
 if pointer:
  aligned=(requested+3)&~3;assert pointer+aligned<=S
  u.mem_write(pointer,bytes([0xcc])*aligned)
 assert u.mem_read(S,512)==before
 # A full-size allocation must leave no allocatable memory, including the reserve.
 if requested==0x1bdf0:
  second,_=launch(u,0x08002b9d,[4],{0x081e3b14:None});assert second==0
 checks+=1
for walk in [0,1]:
 for direction in [1,2,3,4,0xaa000004]:
  u=setup();u.mem_write(S+38,bytes([walk]));u.mem_write(0x0203707d,b'\0')
  u.mem_write(0x02036e48,struct.pack('<hh',10,13))
  stubs={0x08058f78:4,0x0806db84:0,0x08063a20:None,0x0805bba8:3}
  result,events=launch(u,0x09ff1021,[direction],stubs)
  if walk:assert result==0 and not events
  else:
   ref=setup(False);ref.mem_write(0x0203707d,b'\0');ref.mem_write(0x02036e48,struct.pack('<hh',10,13))
   expected,ev=launch(ref,0x0805bb1d,[direction],stubs)
   assert result==expected==3,(result,expected,events,ev)
   assert [(a,args[:2] if a==0x08058f78 else args[:3] if a in [0x0806db84,0x08063a20] else args) for a,args in events]==[(a,args[:2] if a==0x08058f78 else args[:3] if a in [0x0806db84,0x08063a20] else args) for a,args in ev],(events,ev)
  checks+=1
# Readkeys tail thunk preserves callee registers and still invokes RTC/key handler.
u=setup();w32(u,S,0x414d5954);w32(u,S+92,0x43484541);u.mem_write(0x04000130,b'\xff\x03')
_,events=launch(u,0x09ff1001,stubs={0x0891cecc:None,0x0806e6d0:0})
assert [a for a,args in events]==[0x0891cecc,0x0806e6d0,0x0806e6d0]
assert r32(u,S+4)==0x52554e31 and u.mem_read(S+96,1)==b'\x01';checks+=1
print('PASS:',checks,'compiled ROM heap/ABI entry cases; heap headers compared against real original InitHeap')
