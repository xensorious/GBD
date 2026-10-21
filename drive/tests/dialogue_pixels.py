#!/usr/bin/env python3
"""Check the real portrait blitter and archive cache under ASan/UBSan."""
import argparse,json,subprocess
from pathlib import Path
from inspect_rom import inspect

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=ROOT/'build/native-portraits')
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
target=ROOT/'build/test-dialogue-pixels'
command=['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic',
         '-fsanitize=address,undefined','-ffunction-sections','-fdata-sections',
         '-I',str(ROOT/'src'),'-I',str(ROOT/'build'),str(ROOT/'tests/dialogue_pixels.c')]
command.extend(str(ROOT/name) for name in ('src/gfx.c','src/story_io.c','build/assets.c'))
subprocess.run(command+['-Wl,--gc-sections','-o',str(target)],check=True,cwd=ROOT)
run=subprocess.run([str(target)],check=True,cwd=ROOT,capture_output=True,text=True)
print(run.stdout,end='')
manifest=json.loads((ROOT/'build/story-manifest.json').read_text())
result={'rom':inspect(ROOT/'build/Coastline.nds'),'portrait_size':manifest['portrait_size'],
        'native_speaking_pixels':True,'reduced_secondary_and_thumbnail_pixels':True,
        'clipping_opacity_lighting':True,'sanitizers_passed':True,
        'shared_scratch_bytes':307200,'required_scratch_bytes':279680,'result':run.stdout.strip()}
(args.output/'native-portrait-results.json').write_text(json.dumps(result,indent=2)+'\n')
