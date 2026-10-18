"""Assemble the emulator-ready release without shipping tools or test saves."""
from pathlib import Path
from PIL import Image, ImageDraw
import hashlib
import shutil
import zipfile

root=Path(__file__).resolve().parents[1]
build=root/'build'
dist=root/'dist'
dist.mkdir(exist_ok=True)
rom=build/'Bramblebound.gba'
assert rom.exists(),'Build the ROM first.'
shutil.copyfile(rom,dist/'Bramblebound.gba')
instructions=(root/'docs/PLAY.md').read_text()
(dist/'PLAY.txt').write_text(instructions)
images=['starter-selection','garden-helper','rival-battle','first-trail-pin']
sheet=Image.new('RGB',(960,672),'#263b40')
draw=ImageDraw.Draw(sheet)
for i,name in enumerate(images):
    p=build/(name+'.png')
    assert p.exists(),f'Run emulator tests to generate {p.name}.'
    im=Image.open(p).convert('RGB').resize((480,320),Image.Resampling.NEAREST)
    x=(i%2)*480;y=(i//2)*336
    sheet.paste(im,(x,y+16))
    draw.text((x+8,y+2),name.replace('-',' ').upper(),fill='#f4ecd6')
sheet.save(dist/'Preview.png')
names=['Bramblebound.gba','PLAY.txt','Preview.png']
checksums=''.join(hashlib.sha256((dist/n).read_bytes()).hexdigest()+'  '+n+'\n' for n in names)
(dist/'SHA256SUMS.txt').write_text(checksums)
with zipfile.ZipFile(dist/'Bramblebound-First-Light.zip','w',zipfile.ZIP_DEFLATED) as z:
    for n in names+['SHA256SUMS.txt']:z.write(dist/n,'Bramblebound-First-Light/'+n)
print('Packaged ROM, controls, preview and SHA-256 checksums in',dist)
