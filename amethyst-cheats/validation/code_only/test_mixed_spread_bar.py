import struct
from target import ROM, PARTS
from unicorn import *
from unicorn.arm_const import *
PAYLOAD=PARTS
CODE=struct.unpack('<5I',next(b for n,a,b in PAYLOAD if n=='.guards'))[0];END=0x03007c00;BS=0x02030000;SCRIPT=0x02034000

def run(bank,on,busy,damages):
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
 for a,n in [(0x08000000,0x2000000),(0x02000000,0x40000),(0x03000000,0x8000)]:u.mem_map(a,n)
 u.mem_write(0x08000000,ROM)
 u.mem_write(0x080005ec,struct.pack('<I',0x0200cecd));u.mem_write(0x08002b84,struct.pack('<H',0x4710));u.mem_write(0x08002b96,struct.pack('<H',0x0201))
 for name,address,data in PAYLOAD:u.mem_write(address,data)
 def w32(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def w16(a,v):u.mem_write(a,struct.pack('<H',v&0xffff))
 u.mem_write(0x02010c28,bytes([on]));u.mem_write(0x02010c60,b'\x01');w32(0x02023bc8,busy)
 w32(0x0203e038,BS);w32(0x02023d74,SCRIPT)
 u.mem_write(SCRIPT,b'\x0b\x00');u.mem_write(0x02023d6c,bytes([bank]));u.mem_write(0x02023d6b,b'\x01')
 u.mem_write(0x02023bd6,b'\x00\x01\x02\x03');u.mem_write(0x02023bcc,b'\x04')
 w32(0x02023d50,20)
 for i,d in enumerate(damages):
  w32(BS+0x188+i*4,d);w16(0x02023be4+i*0x58,1)
 u.reg_write(UC_ARM_REG_SP,0x03006f00);u.reg_write(UC_ARM_REG_LR,END|1)
 events=[]
 def hook(u,a,n,x):
  if a==END:u.emu_stop();return
  if a==0x088fc524:u.reg_write(UC_ARM_REG_R0,1)
  elif a in [0x088d3a8c,0x088fc5a4]:u.reg_write(UC_ARM_REG_R0,0)
  elif a==0x0800d8b0:
   raw=bytes(u.mem_read(u.reg_read(UC_ARM_REG_R1),u.reg_read(UC_ARM_REG_R2)))
   assert raw[:2]==b'\x18\0',raw
   events.append((u.mem_read(0x02023bc4,1)[0],struct.unpack('<h',raw[2:4])[0]))
  elif a==0x08017248:pass
  else:return
  u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 u.hook_add(UC_HOOK_CODE,hook);u.emu_start(CODE|1,0,count=100000)
 assert u.reg_read(UC_ARM_REG_PC)==END,hex(u.reg_read(UC_ARM_REG_PC))
 actual=struct.unpack('<4i',u.mem_read(BS+0x188,16))
 return actual,events,struct.unpack('<I',u.mem_read(0x02023d74,4))[0]-SCRIPT

checked=0
for bank in range(4):
 for on in [0,1]:
  for busy in [0,1]:
   for ds in [(20,21,22,23),(-20,21,22,-23),(-20,-21,-22,-23)]:
    result,events,advance=run(bank,on,busy,ds)
    expected=tuple(0 if on and not busy and i%2==0 and x>0 else x for i,x in enumerate(ds))
    assert result==expected,(bank,on,busy,ds,result,expected)
    expected_events=[] if busy else [(i,x) for i,x in enumerate(expected) if x]
    assert events==expected_events,(bank,on,busy,ds,events,expected_events)
    assert advance==(0 if busy else 2),(bank,on,busy,advance)
    checked+=1
print('PASS:',checked,'mixed-target spread HP-bar cases with exact ROM DoublesHPBarReduction and EmitHealthBarUpdate')
print('Only IsDoubleSpreadMove, substitute/block queries, buffer-copy and controller-mark routines stubbed.')
