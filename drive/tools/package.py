"""Package the exact validated Coastline cartridge and actual DS captures."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'; DIST=ROOT/'dist'; DIST.mkdir(exist_ok=True)
rom=BUILD/'Coastline.nds'
digest=hashlib.sha256(rom.read_bytes()).hexdigest()
validation=BUILD/'validation'
report=json.loads((validation/'results.json').read_text())
assert report['sha256']==digest,'Validate this exact ROM before packaging.'
assert report['battery_save_reload'],'Battery-save round trip has not passed.'
scenery=json.loads((validation/'scenery-results.json').read_text())
assert scenery['rom']['sha256']==digest,'Capture scenery from this exact ROM.'
migration=json.loads((validation/'migration-results.json').read_text())
assert migration['rom']['sha256']==digest and migration['fresh_process_reload'],'Verify legacy saves with this exact ROM.'
transition=json.loads((validation/'transition-results.json').read_text())
assert transition['rom']['sha256']==digest and transition['biomes_seen']==[1,2],'Drive a region boundary with this exact ROM.'
shutil.copyfile(rom,DIST/rom.name)
(DIST/'PLAY.txt').write_text((ROOT/'README.md').read_text().split('## Build and test')[0].rstrip()+'\n')
shutil.copyfile(ROOT/'CREDITS.txt',DIST/'CREDITS.txt')
(DIST/'VALIDATION.json').write_text(json.dumps({'cartridge':report,'visual_fixture':scenery,'save_migration':migration,'region_transition':transition},indent=2)+'\n')
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',13)
sheet=Image.new('RGB',(1024,1584),'#142b36');d=ImageDraw.Draw(sheet)
for i,(name,title) in enumerate((('race-city','CITY HIGHWAY / LIVE DASHBOARD'),('scenery-coast','PALM COAST'),('run-receipt','ITEMIZED RUN RECEIPT'),('garage','GARAGE AND TUNING'))):
    im=Image.open(validation/(name+'.png')).convert('RGB')
    assert im.size==(256,384),(name,im.size)
    x=(i%2)*512;y=(i//2)*792
    d.text((x+10,y+5),title,font=font,fill='#f4eac8')
    sheet.paste(im.resize((512,768),Image.Resampling.NEAREST),(x,y+24))
sheet.save(DIST/'Preview.png')
location_items=(('city','METRO DAWN'),('coast','PALM COAST'),('causeway','SKYWATER CAUSEWAY'),('pines','PINE RIDGE'),('canyon','REDROCK PASS'),('marina','MARINA NIGHTS'),('wetlands','MANGROVE RUN'),('groves','CITRUS PARKWAY'),('motorcycle-wheelie','KESTREL / WHEELIE'))
locations=Image.new('RGB',(1024,408*((len(location_items)+1)//2)),'#142b36');d=ImageDraw.Draw(locations)
for i,(name,title) in enumerate(location_items):
    path=validation/(('motorcycle-wheelie' if name=='motorcycle-wheelie' else 'scenery-'+name)+'.png')
    im=Image.open(path).convert('RGB');assert im.size==(256,384)
    x=(i%2)*512;y=(i//2)*408
    d.text((x+10,y+5),title,font=font,fill='#f4eac8')
    locations.paste(im.crop((0,0,256,192)).resize((512,384),Image.Resampling.NEAREST),(x,y+24))
locations.save(DIST/'Locations.png')
for name in ('Vehicles.png','Dashboards.png'):
    shutil.copyfile(validation/name,DIST/name)
interface=Image.new('RGB',(1024,1224),'#142b36');d=ImageDraw.Draw(interface)
for i,(name,title) in enumerate((('home','DRIVE CLUB'),('settings','PERSONAL SETTINGS'),('garage-parts','PERMANENT PARTS'),('garage-tuning','CHOOSE YOUR SETUP'),('career','SHARED CAREER'),('dashboard-luxury','A DIFFERENT CLUSTER FOR EACH VEHICLE'))):
    im=Image.open(validation/(name+'.png')).convert('RGB')
    assert im.size==(256,384),(name,im.size)
    x=(i%2)*512;y=(i//2)*408
    d.text((x+10,y+5),title,font=font,fill='#f4eac8')
    interface.paste(im.crop((0,192,256,384)).resize((512,384),Image.Resampling.NEAREST),(x,y+24))
interface.save(DIST/'Interface.png')
shutil.copyfile(validation/'highway-motion.gif',DIST/'highway-motion.gif')
shutil.copyfile(validation/'biome-transition.gif',DIST/'biome-transition.gif')
licenses=DIST/'licenses';licenses.mkdir(exist_ok=True)
for source in (ROOT/'licenses').glob('*.txt'):shutil.copyfile(source,licenses/source.name)
files=['Coastline.nds','PLAY.txt','CREDITS.txt','Preview.png','Locations.png','Vehicles.png','Dashboards.png','Interface.png','highway-motion.gif','biome-transition.gif','VALIDATION.json']+[f'licenses/{p.name}' for p in sorted(licenses.glob('*.txt'))]
(DIST/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256((DIST/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in files))
with zipfile.ZipFile(DIST/'Coastline-DS.zip','w',zipfile.ZIP_DEFLATED) as archive:
    for name in files+['SHA256SUMS.txt']:
        info=zipfile.ZipInfo('Coastline-DS/'+name,date_time=(2026,10,18,0,0,0))
        info.compress_type=zipfile.ZIP_DEFLATED
        archive.writestr(info,(DIST/name).read_bytes())
print('Packaged validated',rom.stat().st_size,'byte DS cartridge:',digest)
