"""Capture toggle, normal key suppression and callback restoration."""
from test_compiled_native_input import Decoder,SEQUENCES,S
import struct
from unicorn.arm_const import *

class Keys(Decoder):
 def hook(self,u,address,size,data):
  if address==0x0891cecc:
   keys=1023^struct.unpack('<H',u.mem_read(0x04000130,2))[0]
   for i in range(5):u.mem_write(0x03003118+2*i,struct.pack('<H',keys))
   if keys:u.mem_write(0x03003124,b'\x01\0')
   u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
  else:super().hook(u,address,size,data)

def capture(d,on):
 for _ in range(30):d.sample(768)
 assert bool(d.uc.mem_read(S+112,1)[0])==on
 for _ in range(90):d.sample(768)
 assert bool(d.uc.mem_read(S+112,1)[0])==on # no repeat while held
 d.sample(256);assert d.uc.mem_read(0x03003118,10)==bytes(10)
 d.sample(0);d.sample(0)

def callbacks(d):
 return tuple(d.r32(0x030030f0+i*4) for i in range(2))

def mode(d):
 return bool(d.uc.mem_read(S+112,1)[0])

def up_taps(d,count):
 for _ in range(count):d.sample(64);d.sample(0)

def normal_keys():
 d=Keys(capture=False)
 for i,value in enumerate(originals):d.w32(0x030030f0+i*4,value)
 return d

d=Keys(capture=False)
originals=(0x08012345,0x08067891)
for i,value in enumerate(originals):d.w32(0x030030f0+i*4,value)
d.enter(SEQUENCES[0]);assert d.calls==[] # commands inert during normal play
d.sample(1);assert d.uc.mem_read(0x03003118,2)==b'\x01\0'
for _ in range(29):
 d.sample(768)
 assert callbacks(d)==originals # Short holds must let battle callbacks run.
 assert d.uc.mem_read(S+115,1)==b'\0' # No release block before a toggle.
assert d.uc.mem_read(S+112,1)==b'\0'
assert d.uc.mem_read(0x03003118,10)==bytes(10)
d.sample(0);d.sample(0)
assert tuple(d.r32(0x030030f0+i*4) for i in range(2))==originals
d.sample(1);assert d.uc.mem_read(0x03003118,2)==b'\x01\0'
d.sample(0)
capture(d,True)
gates=tuple(d.r32(0x030030f0+i*4) for i in range(2))
assert gates[0]==gates[1] and gates!=originals
for key in (1,2,16,32,64,128,256,512,8,4):
 d.uc.mem_write(0x03003124,b'\0\0');d.sample(key)
 assert d.uc.mem_read(0x03003118,10)==bytes(10)
 assert d.uc.mem_read(0x03003124,2)==b'\0\0'
 d.sample(0)
d.enter(SEQUENCES[0]);assert d.calls==[('item',1,1)]
capture(d,False)
assert tuple(d.r32(0x030030f0+i*4) for i in range(2))==originals
d.sample(2);assert d.uc.mem_read(0x03003118,2)==b'\x02\0'
capture(d,True)
d.sample(0,pulse=False)
assert d.uc.mem_read(S+112,1)==b'\0'
assert tuple(d.r32(0x030030f0+i*4) for i in range(2))==originals
capture(d,True)
gate=d.r32(0x030030f0)
d.w32(0x080005ec,0x0891cecd) # Emulator removes hooks before the next ReadKeys.
d.uc.reg_write(UC_ARM_REG_SP,0x03006f00)
d.uc.reg_write(UC_ARM_REG_LR,0x03007c01)
d.uc.emu_start(gate|1,0x03007c00,count=10000)
assert d.uc.mem_read(S+112,1)==b'\0'
assert tuple(d.r32(0x030030f0+i*4) for i in range(2))==originals

# Ten distinct Up presses provide an entry/exit route without a shoulder chord.
up=normal_keys()
up.enter(SEQUENCES[0]);assert up.calls==[]
up_taps(up,9)
assert not mode(up) and callbacks(up)==originals
up.sample(64)
assert mode(up)
up_gates=callbacks(up)
assert up_gates[0]==up_gates[1] and up_gates!=originals
# The final Up must release fully before the command decoder accepts anything.
up.sample(1);up.sample(2)
assert up.calls==[] and up.uc.mem_read(0x03003118,10)==bytes(10)
assert callbacks(up)==up_gates
up.sample(0);up.sample(0)
up.enter(SEQUENCES[0]);assert up.calls==[('item',1,1)]
up_taps(up,9);assert mode(up)
up.sample(64);assert not mode(up)
assert callbacks(up)==up_gates # Exit also waits for every button to release.
up.sample(1);assert callbacks(up)==up_gates
assert up.uc.mem_read(0x03003118,10)==bytes(10)
up.sample(0);up.sample(0)
assert callbacks(up)==originals
up.enter(SEQUENCES[0]);assert up.calls==[('item',1,1)]
up.sample(2);assert up.uc.mem_read(0x03003118,2)==b'\x02\0'

# A held Up counts once, even when it spans many samples.
held=normal_keys()
for _ in range(90):held.sample(64)
assert not mode(held) and callbacks(held)==originals
held.sample(0);up_taps(held,8)
assert not mode(held)
held.sample(64);assert mode(held)
for _ in range(90):held.sample(64)
assert mode(held) # The tenth held press must not toggle back off.
held.sample(0);held.sample(0)

# Other buttons and chords interrupt the Up count. A partial release from
# another button to Up must not start a new press until all buttons are up.
for interruption in (1,65,768):
 interrupted=normal_keys();up_taps(interrupted,9)
 interrupted.sample(interruption);interrupted.sample(64)
 assert not mode(interrupted) and callbacks(interrupted)==originals
 interrupted.sample(0);up_taps(interrupted,9)
 assert not mode(interrupted)
 interrupted.sample(64);assert mode(interrupted)

# A 120-sample interval is accepted; a longer interval resets the partial code.
timely=normal_keys();up_taps(timely,9)
for _ in range(118):timely.sample(0)
timely.sample(64);assert mode(timely)
expired=normal_keys();up_taps(expired,9)
for _ in range(121):expired.sample(0)
expired.sample(64);assert not mode(expired)
expired.sample(0);up_taps(expired,8);assert not mode(expired)
expired.sample(64);assert mode(expired)

selector=Keys()
selector.uc.mem_write(S+38,b'\x01')
for avatar,expected in [(1,0x57414c4b),(2,0x42494b45),(4,0x42494b45),(8,0x57414c4b)]:
 selector.uc.mem_write(0x02037078,bytes([avatar]));selector.sample(0)
 assert selector.r32(S+108)==expected
selector.uc.mem_write(0x03003529,b'\x02');selector.sample(0)
assert selector.r32(S+108)==0x4241544c # Battle hooks retain slot priority.
print('PASS: L+R and Up10 entry/exit, held-key debounce, interruption/timeout, short-chord callback progress, captured-only item grants, key masking and clean callback restoration')
