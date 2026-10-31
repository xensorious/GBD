"""Check the CB2 clone and shared-slot ROM action routing; callees stubbed."""
from pathlib import Path
import sys, struct
from target import PROJECT, ROM, PARTS as COMPILED_PARTS
sys.path.insert(0, str(PROJECT / 'tools'))
from code_only_relocate import clone_parts, _instructions, ACTION_SOURCE, ACTION_END, CB2_SOURCE, CB2_END, CB2_DEST
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *

END=0x03007C00
GUARD=struct.unpack_from('<I',next(data for name,_,data in COMPILED_PARTS if name=='.callbacks'))[0]
PARTS=clone_parts(ROM,GUARD)
assert [name for name,_,_ in PARTS]==['.cb2clone']
assert len(PARTS[0][2])<=0x140
REGS=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]

def run(which,cloned,options):
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
 for a,n in [(0x08000000,0x2000000),(0x02000000,0x40000),(0x03000000,0x8000),(0x04000000,0x1000)]:u.mem_map(a,n)
 u.mem_write(0x08000000,ROM)
 for _,a,data in PARTS:u.mem_write(a,data)
 def w32(a,v):u.mem_write(a,struct.pack('<I',v))
 def w16(a,v):u.mem_write(a,struct.pack('<H',v))
 def w8(a,v):u.mem_write(a,bytes([v]))
 w32(0x0203e038,0x02030000);w32(0x02023fe8,0x02031000)
 w8(0x02023bcc,options.get('banks',2));w8(0x02023be3,options.get('action',0))
 w32(0x02022b4c,options.get('battleflags',0));w16(0x0300311c,options.get('mainkeys',0))
 w16(0x0300311a,0x1234);w16(0x0300311e,0x5678)
 w32(0x030030f4,0x08011101)
 w8(0x0203010d,options.get('phase',0));w8(0x020301c8,options.get('raid_phase',0))
 w8(0x02023e8a,options.get('outcome',0));w8(0x02031048,options.get('turn',0))
 w8(0x02023be2,options.get('turnid',0))
 u.mem_write(0x02023bda,bytes([0,1,2,3]));u.mem_write(0x02023bde,bytes([0,0,0,0]))
 for bank in range(4):
  w16(0x02023be4+bank*0x58,1);w16(0x02023be4+bank*0x58+0x28,20)
  w16(0x02023be4+bank*0x58+0x2c,20)
 u.mem_write(0x02010038,ROM[0x250038:0x250038+56])
 if options.get('override_action'):w32(0x02010038+40,GUARD)
 # ExecuteActions stays in ROM. Only its action-table literal changes while
 # this phase owns AR slot 4; the ordinary table entries must behave identically.
 if which=='actions' and cloned:u.mem_write(0x088ce782,struct.pack('<H',0x0201))
 _,action_calls,_=_instructions(ROM,ACTION_SOURCE,ACTION_END)
 _,cb_calls,_=_instructions(ROM,CB2_SOURCE,CB2_END)
 calls=set(action_calls.values())|set(cb_calls.values())
 calls-={0x08850436,0x088cff68,0x088cff6c,0x088cd16c,0x088cd16e}
 # All dynamically called actions and function literals are also intercepted.
 calls|={x&~1 for x in struct.unpack_from('<14I',ROM,0x250038)}
 calls|={GUARD&~1,0x0806e568,0x080123e4,0x08070a84,0x08070588,0x08000544,
         0x080751c4,0x08017544,0x0801bbe4}
 events=[];budget=[0]
 def hook(u,a,n,_):
  budget[0]+=1
  if a==END:u.emu_stop();return
  if a in calls:
   normalized=0x080123e4 if which=='cb2' and a==(GUARD&~1) else a
   args=tuple(u.reg_read(r) for r in REGS)
   # R3 is an indirect-call target; normalize the intentionally changed alias.
   args=tuple(0x080123e5 if which=='cb2' and x==GUARD else
              0x08011101 if x==(CB2_DEST|1) else x for x in args)
   events.append((normalized,args))
   if normalized==0x0806e568:ret=options.get('speed',0)
   else:ret=options.get('external_return',0)
   if normalized==0x080123e4 and options.get('change_callback'):
    w32(0x030030f4,0x08012345)
   if normalized==0x08000544:w32(0x030030f4,u.reg_read(UC_ARM_REG_R0))
   u.reg_write(UC_ARM_REG_R0,ret)
   u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 u.hook_add(UC_HOOK_CODE,hook)
 u.reg_write(UC_ARM_REG_SP,0x03007000);u.reg_write(UC_ARM_REG_LR,END|1)
 for i,r in enumerate([UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]):u.reg_write(r,0x12340000+i)
 entry=({'actions':ACTION_SOURCE|1,'cb2':CB2_DEST|1} if cloned else {'actions':ACTION_SOURCE|1,'cb2':CB2_SOURCE|1})[which]
 u.emu_start(entry,0,count=100000)
 assert u.reg_read(UC_ARM_REG_PC)==END,(which,cloned,options,hex(u.reg_read(UC_ARM_REG_PC)),events[-4:])
 assert u.reg_read(UC_ARM_REG_SP)==0x03007000
 assert [u.reg_read(r) for r in [UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]]==[0x12340000+i for i in range(8)]
 if cloned and u.mem_read(0x030030f4,4)==struct.pack('<I',CB2_DEST|1):w32(0x030030f4,0x08011101)
 return events,bytes(u.mem_read(0x02000000,0x40000)),bytes(u.mem_read(0x03000000,0x6c00))

checks=0
for options in [dict(speed=s,battleflags=f,change_callback=c) for s in range(6) for f in [0,0x2,0x10000] for c in [False,True]]:
 original=run('cb2',False,options);cloned=run('cb2',True,options)
 assert original==cloned,('cb2',options,original[0],cloned[0])
 checks+=1
print('PASS:',checks,'CB2 differential cases including fast loops',flush=True)
for options in [dict(banks=b,phase=p,raid_phase=q,action=a,external_return=ret)
                for b in [2,4] for p in [0,1] for q in range(7)
                for a in [0,10,11,12] for ret in [0,1]]:
 original=run('actions',False,options);cloned=run('actions',True,options)
 assert original==cloned,('actions',options,original[0],cloned[0])
 checks+=1
print('PASS:',checks,'total callback/action-table differential cases; external callees stubbed',flush=True)
original=run('actions',False,{'action':10,'override_action':True,'phase':1,'raid_phase':5})
cloned=run('actions',True,{'action':10,'override_action':True,'phase':1,'raid_phase':5})
assert any(a==0x0801D030 for a,args in original[0]), original[0]
assert any(a==GUARD&~1 for a,args in cloned[0]), cloned[0]
assert not any(a==0x0801D030 for a,args in cloned[0]), cloned[0]
print('PASS: original ROM ExecuteActions dispatches action10 through the shared-slot RAM table',flush=True)
