"""Target-backed tests for compiled runtime; no Delta-device claim."""
import struct
from target import ROM, PARTS
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0
S=0x02010C00; END=0x03007C00; SCRIPT=0x02030000
PAYLOAD=PARTS
def setup():
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
 for a,n in [(0x08000000,0x02000000),(0x02000000,0x40000),(0x03000000,0x8000),(0x04000000,0x1000)]:u.mem_map(a,n)
 u.mem_write(0x08000000,ROM)
 u.mem_write(0x080005ec,struct.pack('<I',0x0200cecd));u.mem_write(0x08002b84,struct.pack('<H',0x4710));u.mem_write(0x08002b96,struct.pack('<H',0x0201))
 for name,a,b in PAYLOAD:u.mem_write(a,b)
 return u
def w32(u,a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def w16(u,a,v):u.mem_write(a,struct.pack('<H',v&0xffff))
def r32(u,a):return struct.unpack('<I',u.mem_read(a,4))[0]
def r16(u,a):return struct.unpack('<H',u.mem_read(a,2))[0]
def launch(u,ptr,stubs,events):
 u.reg_write(UC_ARM_REG_SP,0x03006f00);u.reg_write(UC_ARM_REG_LR,END|1)
 def hook(u,a,n,x):
  if a==END:u.emu_stop()
  elif a in stubs:
   events.append(a)
   if stubs[a] is not None:u.reg_write(UC_ARM_REG_R0,stubs[a])
   u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 handle=u.hook_add(UC_HOOK_CODE,hook)
 u.emu_start(ptr|1,0,count=100000)
 assert u.reg_read(UC_ARM_REG_PC)==END,hex(u.reg_read(UC_ARM_REG_PC))
 u.hook_del(handle)
u=setup();w32(u,S,0x414d5954);w16(u,0x04000130,1023)
launch(u,0x0200cecd,{0x0891cecc:None,0x0806e6d0:0},[])
assert r32(u,S+4)==0x52554e31
GUARDS=dict(zip([0x0b,0x0c,0x19,0x79,0xf4], struct.unpack('<5I',u.mem_read(0x02010b00,20))))
u.mem_write(S+96,b'\x01')
BASE_RAM=bytes(u.mem_read(0x02000000,0x40000))
def run(op,bank,damage,on,busy=False):
 u=setup();u.mem_write(0x02000000,BASE_RAM)
 u.mem_write(S+40,bytes([on]));u.mem_write(SCRIPT,bytes([op,0,0,0,0,0,0]))
 w32(u,0x02023d74,SCRIPT);w32(u,0x02023d50,damage);w32(u,0x02023bc8,int(busy))
 w32(u,0x0203e038,0x0203a000)
 u.mem_write(0x02023bd6,bytes([0,1,2,3]));u.mem_write(0x02023bcc,b'\x04')
 u.mem_write(0x02023d6b,bytes([bank]));u.mem_write(0x02023d6c,bytes([bank]))
 for i in range(4):
  mon=0x02023be4+i*0x58;w16(u,mon,1);w16(u,mon+0x28,50);w16(u,mon+0x2c,100)
 stubs={0x088d3b80:0,0x088edb8c:0,0x0800dff0:None,0x08017248:None}
 if op==0x0b:stubs[0x088fd308]=None;stubs[0x088fc524]=0
 if op==0x19:stubs[0x088fe134]=None
 events=[];launch(u,GUARDS[op],stubs,events)
 return u,events
checked=0
for op in [0x0c,0x79,0xf4]:
 for bank in range(4):
  for on in [0,1]:
   for damage in ([20,999,-20,0] if op!=0x79 else [20]):
    u,events=run(op,bank,damage,on)
    protected=on and bank%2==0 and (op==0x79 or damage>=0)
    expected=50 if protected else (0 if op==0x79 else min(100,max(0,50-damage)))
    # Vanilla command F4 subtracts signed damage without maxHP clamping.
    if op==0xf4 and not protected:expected=(50-damage)&0xffff
    hp=r16(u,0x02023be4+bank*0x58+0x28)
    assert hp==expected,(op,bank,on,damage,hp,expected)
    assert r32(u,0x02023d74)==SCRIPT+(2 if op==0x0c else 1)
    checked+=1
 for bank in range(4):
  if op==0xf4:continue
  u,ev=run(op,bank,20,1,True)
  assert r16(u,0x02023be4+bank*0x58+0x28)==50
  assert r32(u,0x02023d74)==SCRIPT
  checked+=1
for bank in range(4):
 for on in [0,1]:
  for damage in [20,999,-20,0]:
   for busy in [False,True]:
    u,ev=run(0x0b,bank,damage,on,busy)
    protected=on and bank%2==0 and damage>=0 and not busy
    assert (0x088fd308 in ev)==(not protected)
    assert r32(u,0x02023d74)==SCRIPT+(2 if protected else 0)
    checked+=1
# Faint guard restores a protected previously living slot before its original
# callback, but leaves a slot that was not marked living alone.
for bank in range(4):
 for on in [0,1]:
  for alive in [0,1]:
   u=setup();u.mem_write(0x02000000,BASE_RAM)
   u.mem_write(S+40,bytes([on, (1<<bank) if alive else 0]));w16(u,S+42+bank*2,37)
   u.mem_write(0x02023bd6,bytes([0,1,2,3]));w16(u,0x02023bce+bank*2,bank);w16(u,S+100+bank*2,bank)
   w16(u,0x02023be4+bank*0x58+0x28,0)
   events=[];launch(u,GUARDS[0x19],{0x088fe134:None},events)
   expected=37 if on and alive and bank%2==0 else 0
   assert r16(u,0x02023be4+bank*0x58+0x28)==expected
   if expected:assert r16(u,0x02024284+bank*100+86)==37
   assert events==[0x088fe134]
   checked+=1
# A bank that switched to a different party slot must not inherit its old alive bit.
for bank in [0,2]:
 u=setup();u.mem_write(0x02000000,BASE_RAM)
 u.mem_write(S+40,bytes([1,1<<bank]));w16(u,S+42+bank*2,37)
 w16(u,S+100+bank*2,bank);w16(u,0x02023bce+bank*2,(bank+1)%6)
 u.mem_write(0x02023bd6,bytes([0,1,2,3]));w16(u,0x02023be4+bank*0x58+0x28,0)
 events=[];launch(u,GUARDS[0x19],{0x088fe134:None},events)
 assert r16(u,0x02023be4+bank*0x58+0x28)==0 and events==[0x088fe134]
 checked+=1
print('Compiled guard pointers:',{hex(k):hex(v) for k,v in GUARDS.items()})
print('PASS:',checked,'compiled-native effect cases')
print('Original ROM datahp/direct-self-HP paths execute; four downstream callees stubbed. Healthbar/faint originals stubbed to verify routing/restoration.')
