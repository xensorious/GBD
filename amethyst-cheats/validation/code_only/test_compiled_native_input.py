"""Compiled ARM input/command emission tests, external game effects stubbed."""
import struct
from target import ROM, PARTS
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
S=0x02010C00; END=0x03007C00
PREFIX=[512,256,512,256,32,16,64,128]
SEQUENCES=[PREFIX+[1,1],PREFIX+[2,2],PREFIX+[1,2],PREFIX+[2,1],PREFIX+[512,512],PREFIX+[256,256]]
class Decoder:
 def __init__(self,bag_success=1,capture=True):
  self.uc=u=Uc(UC_ARCH_ARM,UC_MODE_THUMB);self.calls=[];self.bag_success=bag_success;self.shiny=0
  for a,n in [(0x08000000,0x02000000),(0x02000000,0x40000),(0x03000000,0x8000),(0x04000000,0x1000)]:u.mem_map(a,n)
  u.mem_write(0x08000000,ROM)
  u.mem_write(0x080005ec,struct.pack('<I',0x0200cecd));u.mem_write(0x08002b84,struct.pack('<H',0x4710));u.mem_write(0x08002b96,struct.pack('<H',0x0201))
  for name,a,b in PARTS:u.mem_write(a,b)
  self.w32(S,0x414d5954);self.w32(0x0203988c,0x0203bb20)
  if capture:u.mem_write(S+112,b'\x01')
  u.hook_add(UC_HOOK_CODE,self.hook)
 def w32(self,a,v):self.uc.mem_write(a,struct.pack('<I',v))
 def r32(self,a):return struct.unpack('<I',self.uc.mem_read(a,4))[0]
 def hook(self,u,a,n,x):
  if a==END:u.emu_stop();return
  if a==0x0891cecc:pass
  elif a==0x0806e6d0:u.reg_write(UC_ARM_REG_R0,self.shiny)
  elif a in [0x0806e680,0x0806e6a8]:
   assert u.reg_read(UC_ARM_REG_R0)==0x913
   self.shiny=int(a==0x0806e680);self.calls.append(('shiny',self.shiny))
  elif a==0x0809a084:
   self.calls.append(('item',u.reg_read(UC_ARM_REG_R0),u.reg_read(UC_ARM_REG_R1)))
   u.reg_write(UC_ARM_REG_R0,self.bag_success)
  else:return
  u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 def sample(self,mask,pulse=True):
  u=self.uc;u.mem_write(0x04000130,struct.pack('<H',1023^mask))
  u.reg_write(UC_ARM_REG_SP,0x03006f00);u.reg_write(UC_ARM_REG_LR,END|1)
  self.w32(S+92,0x43484541 if pulse else 0)
  u.emu_start(0x0200cecd,0,count=100000)
  assert u.reg_read(UC_ARM_REG_PC)==END
 def enter(self,seq,hold=1):
  for key in seq:
   for i in range(hold):self.sample(key)
   self.sample(0)
 def open(self):
  if self.uc.mem_read(S+112,1)==b'\0':
   for _ in range(30):self.sample(768)
   self.sample(0);self.sample(0)
checks=0
for index,seq in enumerate(SEQUENCES):
 d=Decoder();d.enter(seq);d.enter(seq)
 assert [d.r32(S+52+i*4) for i in range(6)]==[2 if i==index else 0 for i in range(6)]
 if index<3:assert d.calls==[('item',[1,25,68][index],1)]*2 and d.r32(S+76+index*4)==2
 if index==3:assert d.r32(S+8)==0
 if index==4:assert d.calls==[('shiny',1),('shiny',0)]
 if index==5:assert d.uc.mem_read(S+40,1)==b'\0'
 checks+=1
# Holding all keys for multiple frames and the final key for 240 frames.
d=Decoder();d.enter(SEQUENCES[0][:-1],hold=4);d.sample(1)
for i in range(240):d.sample(1)
assert d.calls==[('item',1,1)] and d.r32(S+52)==1;checks+=1
# A full bag fails once without retrying or emitting any other item request.
d=Decoder(bag_success=0);d.enter(SEQUENCES[2]);assert d.calls==[('item',68,1)] and d.r32(S+84)==0;checks+=1
# Shiny must alternate on/off/on in the actual compiled handler.
d=Decoder();[d.enter(SEQUENCES[4]) for _ in range(3)];assert d.calls==[('shiny',1),('shiny',0),('shiny',1)];checks+=1
# Chords reject sequence and suppress partial release until every key is up.
d=Decoder();d.enter(PREFIX[:4]);d.sample(3);d.sample(1);d.sample(2);d.sample(0);d.enter(PREFIX[4:]+[1,1]);assert d.calls==[]
d.enter(SEQUENCES[0]);assert d.calls==[('item',1,1)];checks+=1
# Mispress resets but overlapping L/R prefix can recover.
d=Decoder();d.enter(PREFIX[:4]+[8]+PREFIX[4:]+[1,1]);assert d.calls==[]
d.enter([512,256]+SEQUENCES[0]);assert d.calls==[('item',1,1)];checks+=1
# Long no-input pause invalidates the partial sequence.
d=Decoder();d.enter(PREFIX[:4]);[d.sample(0) for _ in range(121)];d.enter(PREFIX[4:]+[1,1]);assert d.calls==[];checks+=1
# Disabling restores original shiny setting and the next entry toggles that actual setting.
for initial in [0,1]:
 d=Decoder();d.shiny=initial;d.enter(SEQUENCES[4]);assert d.shiny==1-initial
 d.sample(0,pulse=False);assert d.shiny==initial
 d.open();d.enter(SEQUENCES[4]);assert d.shiny==1-initial
 checks+=1
# The real battle cleanup clears flag0913. Explicit ON must restore it on
# the next input sample, repeatedly; a second command must stop restoration.
d=Decoder();d.enter(SEQUENCES[4]);assert d.shiny==1
for _ in range(3):
 d.shiny=0;d.sample(0);assert d.shiny==1
d.enter(SEQUENCES[4]);assert d.shiny==0
d.sample(0);assert d.shiny==0
d.enter(SEQUENCES[4]);d.shiny=0;d.sample(0);assert d.shiny==1
checks+=1
# A flag present before any cheat command can be a transient game effect.
# Clearing that effect must not silently make it persistent.
d=Decoder();d.shiny=1;d.sample(0);d.shiny=0;d.sample(0);assert d.shiny==0
checks+=1
print('PASS:',checks,'compiled-native input/command scenarios')
print('Items are emitted with quantity1; FlagSet/FlagClear alternate; external item/shiny operations are stubbed here.')
