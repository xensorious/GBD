"""Package the tested DS cartridge with actual-emulator screenshots and controls."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import hashlib, json, shutil, zipfile
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build';DIST=ROOT/'dist';DIST.mkdir(exist_ok=True)
rom=BUILD/'Asterwild.nds';assert rom.exists(),'Build the DS ROM first.'
rom_hash=hashlib.sha256(rom.read_bytes()).hexdigest()
reports=[BUILD/'validation'/'final-visual-results.json',BUILD/'validation'/'results.json']
assert any(p.exists() and json.loads(p.read_text()).get('rom',{}).get('sha256')==rom_hash for p in reports), 'Run emulator validation against this exact ROM before packaging.'
shutil.copyfile(rom,DIST/rom.name)
(DIST/'PLAY.txt').write_text((ROOT/'README.md').read_text().split('## Build and test')[0].rstrip()+'\n')
(DIST/'CREDITS.txt').write_text((ROOT/'CREDITS.txt').read_text())
shots=BUILD/'validation'/'screens'
scenes=[('harbor-arrival','SUNWAKE HARBOR'),('starter-ferro','YOUR FIRST COMPANION'),
        ('keeper-trial','THE LANTERN TRIAL'),('focused-map','BUTTON-CONTROLLED NOTEBOOK')]
scale=2;tilew=256*scale;tileh=384*scale+24
sheet=Image.new('RGB',(tilew*2,tileh*2),'#142b40');draw=ImageDraw.Draw(sheet)
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',13)
for i,(name,label) in enumerate(scenes):
    path=shots/(name+'.png');assert path.exists(),f'Run make test first: missing {name}'
    im=Image.open(path).convert('RGB');assert im.size==(256,384),f'Unexpected DS screenshot: {im.size}'
    x=(i%2)*tilew;y=(i//2)*tileh
    draw.text((x+10,y+5),label,font=font,fill='#f7e8c4')
    sheet.paste(im.resize((tilew,384*scale),Image.Resampling.NEAREST),(x,y+24))
sheet.save(DIST/'Preview.png')
licenses=DIST/'licenses';licenses.mkdir(exist_ok=True)
for file in (ROOT/'licenses').glob('*.txt'):shutil.copyfile(file,licenses/file.name)
files=['Asterwild.nds','PLAY.txt','Preview.png','CREDITS.txt']+[f'licenses/{p.name}' for p in sorted(licenses.glob('*.txt'))]
(DIST/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256((DIST/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in files))
with zipfile.ZipFile(DIST/'Asterwild-DS.zip','w',zipfile.ZIP_DEFLATED) as archive:
    for name in files+['SHA256SUMS.txt']:
        info=zipfile.ZipInfo('Asterwild-DS/'+name,date_time=(2026,10,18,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
        archive.writestr(info,(DIST/name).read_bytes())
print('Packaged',rom.stat().st_size,'byte DS ROM in',DIST)
