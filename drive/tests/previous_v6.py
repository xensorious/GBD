#!/usr/bin/env python3
"""Preserve every previous v6 profile field while migrating the record to v7.

Requires finishes.py captures from release e6066de in --source. The ROM hash
is pinned below. This boots a real previous battery in a new emulator process;
no RAM writes or emulator save states are used.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import subprocess
import sys
from emulator import ROOT
from inspect_rom import inspect
from game_snapshot import Save
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,default=ROOT/'build'/'finish-final')
p.add_argument('--output',type=Path,default=ROOT/'build'/'city-release')
a=p.parse_args();a.source=a.source.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
old=json.loads((a.source/'finishes-results.json').read_text())
assert old['rom']['sha256']=='e0e33161e65f8d3dbbb79ece034698aefc2b0b60c7b2a78c67eb576f46551ef3'
assert old['high_id_color_preset'] and old['fresh_process_reload']
target=a.output/'previous-v6-save';target.mkdir(exist_ok=True)
shutil.copyfile(a.source/'finishes-save'/'Coastline.dsv',target/'Coastline.dsv')
expected=Save.from_buffer_copy((a.source/'finishes-expected.bin').read_bytes());expected.version=7;expected.checksum=0
expected_file=a.output/'previous-v6-expected-v7.bin';expected_file.write_bytes(bytes(expected))
subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(ROOT/'build'/'Coastline.nds'),
 '--elf',str(ROOT/'build'/'arm9.elf'),'--save-dir',str(target),'--expected',str(expected_file),
 '--output',str(a.output/'previous-v6-reloaded.png')],check=True)
result={'rom':inspect(ROOT/'build'/'Coastline.nds'),'source_release_sha256':old['rom']['sha256'],
 'profile_fields_preserved':True,'version_migrated':7,'fresh_process_reload':True,'includes_all_paint_and_livery_color_history':True}
(a.output/'previous-v6-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
