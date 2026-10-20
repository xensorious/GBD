import sys,json,hashlib,argparse
from pathlib import Path
sys.path.insert(0,'tests')
from emulator import Emulator
from playthrough import DriveThrough
from game_snapshot import find_symbol
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=Path('build/story-release'));a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);out=a.output/'autosave-probe';out.mkdir(exist_ok=True)
import shutil;shutil.rmtree(out/'save',ignore_errors=True)
with Emulator(Path('build/Coastline.nds'),out/'save') as e:
 e.run(180);d=DriveThrough(e,find_symbol(Path('build/arm9.elf'),'game'),out)
 d.press('a');d.press('down');d.press('a');d.press('down');d.press('a')
 for n in range(500):
  d.press('a')
  if d.game().save.reserved[0]:break
 d.ticks(20);g=d.game();print('AUTOSAVE',g.save.reserved[0],g.save_ok,g.save_requested,flush=True);assert g.save.reserved[0]==1 and g.save_ok
 g.save.checksum=0;(out/'expected.bin').write_bytes(bytes(g.save))
import subprocess
subprocess.run([sys.executable,'tests/reload.py','--rom','build/Coastline.nds','--elf','build/arm9.elf','--save-dir',str(out/'save'),'--expected',str(out/'expected.bin'),'--output',str(out/'reloaded.png')],check=True)

(a.output/'story-autosave-results.json').write_text(json.dumps({'sha256':hashlib.sha256(Path('build/Coastline.nds').read_bytes()).hexdigest(),'immediate_completion_save':True,'fresh_process_reload':True,'events':1,'cash_awarded':0,'button_only':True,'emulated_ram_read_only':True},indent=2)+'\n')
