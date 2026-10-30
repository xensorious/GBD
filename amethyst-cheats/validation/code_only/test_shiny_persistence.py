"""Real flag cleanup and real generation with the compiled shiny toggle.

Only the RTC/keypad implementation is stubbed. FlagGet/Set/Clear, CreateMon,
the exact end-battle flag-clearing loop and the compiled handler execute.
"""
import struct
from target import ROM, PARTS
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *

S=0x02010c00;END=0x0203ff00;MON=0x0202402c
SEQUENCE=[512,256,512,256,32,16,64,128,512,512]
PID=0x10203040;OT=0x99887766
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for address,size in ((0x08000000,0x02000000),(0x02000000,0x40000),
                     (0x03000000,0x8000),(0x04000000,0x1000)):
 u.mem_map(address,size)
u.mem_write(0x08000000,ROM)
for name,address,data in PARTS:u.mem_write(address,data)
def w32(address,value):u.mem_write(address,struct.pack('<I',value))
w32(0x03005000,0xdeadbeef)
u.mem_write(0x03005008,struct.pack('<II',0x02018000,0x02010000))
w32(0x080005ec,0x0200cecd)
u.mem_write(0x08002b84,struct.pack('<H',0x4710))
u.mem_write(0x08002b96,struct.pack('<H',0x0201))
w32(S,0x414d5954)
def hook(cpu,address,size,_):
 if address==0x0891cecc:cpu.reg_write(UC_ARM_REG_PC,cpu.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,hook)
def call(address,args=(),stack=()):
 u.reg_write(UC_ARM_REG_SP,0x03007c00);u.reg_write(UC_ARM_REG_LR,END|1)
 for register,value in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):
  u.reg_write(register,value)
 if stack:u.mem_write(0x03007c00,struct.pack('<'+'I'*len(stack),*stack))
 u.emu_start(address|1,END,count=2000000)
 assert u.reg_read(UC_ARM_REG_PC)==END
 return u.reg_read(UC_ARM_REG_R0)
def flag():return call(0x0806e6d1,(0x913,))
def sample(keys=0):
 u.mem_write(0x04000130,struct.pack('<H',1023^keys));w32(S+92,0x43484541)
 call(0x0200cecd)
def enter():
 for key in SEQUENCE:sample(key);sample()
def generate():
 call(0x0803da55,(MON,1,5,31),(1,PID,1,OT))
 pid,owner=struct.unpack('<II',u.mem_read(MON,8))
 assert owner==OT
 return ((pid&65535)^(pid>>16)^(owner&65535)^(owner>>16))<16
def cleanup():
 # Exact loop clears the 17 real flags, including0913. Its original local
 # register setup and callee trampoline are included in execution.
 u.reg_write(UC_ARM_REG_SP,0x03007c00)
 u.emu_start(0x088f033b,0x088f034e,count=100000)
 assert u.reg_read(UC_ARM_REG_PC)==0x088f034e

assert not flag() and not generate()
enter();assert flag() and u.mem_read(S+39,1)==b'\x01'
for battle in range(3):
 for _ in range(3):assert generate()
 cleanup();assert not flag()  # Confirms the original game's one-battle clear.
 sample();assert flag() and u.mem_read(S+39,1)==b'\x01'
assert struct.unpack('<I',u.mem_read(S+68,4))[0]==1
enter();assert not flag() and not generate()
cleanup();sample();assert not flag() and not generate()
enter();assert flag() and generate()
cleanup();sample();assert flag() and generate()
print('PASS: one ON entry survives 3 real cleanup loops and 9 real generations; OFF restores normal, third entry re-enables')
