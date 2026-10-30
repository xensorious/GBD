"""Actual player collision return patch across terrain/result/direction inputs.

Map lookup and lower collision dependencies are stubbed. This verifies the
shared return path; it does not replace live-map testing of scripts and warps.
"""
import struct
from target import ROM
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *

u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
for base,size in ((0x08000000,0x02000000),(0x02000000,0x40000),(0x03000000,0x8000)):
 u.mem_map(base,size)
u.mem_write(0x08000000,ROM)
assert ROM[0x5bb8c:0x5bb8e]==b'\x00\x06'
END=0x03007c00
u.mem_write(0x0203707d,b'\0')
u.mem_write(0x02036e48,struct.pack('<hh',10,13))
behavior=collision=0
def hook(cpu,address,size,_):
 if address==END:cpu.emu_stop();return
 values={0x08058f78:behavior,0x0806db84:0,0x08063a20:0,0x0805bba8:collision}
 if address in values:
  cpu.reg_write(UC_ARM_REG_R0,values[address])
  cpu.reg_write(UC_ARM_REG_PC,cpu.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,hook)
checked=0
for enabled in (False,True):
 u.mem_write(0x0805bb8c,b'\x00\x20' if enabled else b'\x00\x06')
 u.ctl_remove_cache(0x0805bb1c,0x0805bba8)
 for behavior in range(256):
  for collision in (0,1,2,3,4,5,6,7,8,9,10):
   for direction in (1,2,3,4):
    u.reg_write(UC_ARM_REG_R0,direction)
    u.reg_write(UC_ARM_REG_SP,0x03006f00);u.reg_write(UC_ARM_REG_LR,END|1)
    u.emu_start(0x0805bb1d,END,count=10000)
    result=u.reg_read(UC_ARM_REG_R0)
    assert u.reg_read(UC_ARM_REG_PC)==END and u.reg_read(UC_ARM_REG_SP)==0x03006f00
    if enabled:assert result==0,(behavior,collision,direction,result)
    checked+=1
print('PASS',checked,'collision-return cases across 256 behavior bytes, 11 collision inputs and 4 directions')
