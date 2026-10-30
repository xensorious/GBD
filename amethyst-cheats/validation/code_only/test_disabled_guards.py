"""Liveness checks must fail open if either engine hook is gone."""
from test_compiled_native_effects import *
original_setup=setup
checked_off=0
for missing in ['readkeys','heap']:
 def off_setup():
  uc=original_setup()
  if missing=='readkeys':uc.mem_write(0x080005ec,struct.pack('<I',0x0891cecd))
  else:uc.mem_write(0x08002b84,struct.pack('<H',0x6010))
  return uc
 # run() belongs to the imported module, so replace its actual globals binding.
 run.__globals__['setup']=off_setup
 for op in [0x0c,0x79,0xf4]:
  for bank in range(4):
   uc,ev=run(op,bank,20,1)
   assert r16(uc,0x02023be4+bank*0x58+0x28)==(0 if op==0x79 else 30)
   checked_off+=1
 for bank in [0,2]:
  uc,ev=run(0x0b,bank,20,1)
  assert ev==[0x088fd308]
  checked_off+=1
  uc=off_setup();uc.mem_write(0x02000000,BASE_RAM)
  uc.mem_write(S+40,bytes([1,1<<bank]));w16(uc,S+42+bank*2,37)
  w16(uc,S+100+bank*2,bank);w16(uc,0x02023bce+bank*2,bank)
  uc.mem_write(0x02023bd6,bytes([0,1,2,3]));w16(uc,0x02023be4+bank*0x58+0x28,0)
  events=[];launch(uc,GUARDS[0x19],{0x088fe134:None},events)
  assert r16(uc,0x02023be4+bank*0x58+0x28)==0 and events==[0x088fe134]
  checked_off+=1
run.__globals__['setup']=original_setup
print('PASS',checked_off,'missing-ROM-hook battle guards fail open')
