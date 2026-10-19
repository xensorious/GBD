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
assert len(report['native'])==10,'Run all ten native sanitizer suites before packaging.'
assert report['long_drive_cadence']['updates']>0 and report['long_drive_cadence']['hz']>=28,'Verify update cadence across the longer stock drive.'
handling=json.loads((validation/'handling-results.json').read_text())
assert handling['rom']['sha256']==digest,'Validate handling and pursuit with this exact ROM.'
for check in ('launch_grace_passed','stopped_catch_passed','pause_frozen','slow_mode_safe',
              'quarter_combo_passed','controlled_drift_passed','ticket_banked_once'):
    assert handling[check],f'Handling check has not passed: {check}'
assert handling['collision_witnesses']['clear_pass']['passed'] and handling['collision_witnesses']['real_contact']['passed'],'Verify clear passes and genuine vehicle contact.'
scenery=json.loads((validation/'scenery-results.json').read_text())
assert scenery['rom']['sha256']==digest and scenery['fresh_process_reload'],'Capture scenery and reload the review profile from this exact ROM.'
assert scenery['dashboard_vehicle_skin_combinations']==43,'Review all 31 vehicles and representative skins for each dashboard family.'
assert len(scenery['finite_turbo_checks'])==3,'Verify depletion, recovery, drift and wheelie recharge.'
assert {item['screenshot'] for item in scenery['water_contact_captures']}=={'grounded-boat.png','grounded-pier.png'},'Capture the actual grounded water objects.'
migration=json.loads((validation/'migration-results.json').read_text())
assert migration['rom']['sha256']==digest and migration['fresh_process_reload'],'Verify legacy saves with this exact ROM.'
for key in ('previous_v2_compatibility','previous_turbo_v2_compatibility','previous_handling_v2_compatibility'):
    migrated=migration[key]
    assert migrated['legacy_fields_preserved'] and migrated['new_expansion_defaults'] and migrated['fresh_process_reload'],f'Preserve, migrate and reload the released save: {key}'
    assert migrated['serialized_save_bytes']==500 and migrated['physical_save_bytes']==512,'Keep old battery files compatible without changing the save-chip capacity.'
transition=json.loads((validation/'transition-results.json').read_text())
assert transition['rom']['sha256']==digest and transition['biomes_seen']==[1,2],'Drive a region boundary with this exact ROM.'
expansion=json.loads((validation/'expansion-results.json').read_text())
assert expansion['rom']['sha256']==digest,'Validate the expansion with this exact ROM.'
for check in ('class_gates_passed','cosmetics_passed','contracts_passed','preview_stable'):
    assert expansion[check],f'Expansion check has not passed: {check}'
assert expansion['serialized_save_bytes']==500,'Preserve the original battery-save chip capacity.'
assert expansion['vehicles_reviewed']==31 and expansion['locations_reviewed']==13,'Review the complete expanded selection.'
shutil.copyfile(rom,DIST/rom.name)
(DIST/'PLAY.txt').write_text((ROOT/'README.md').read_text().split('## Build and test')[0].rstrip()+'\n')
shutil.copyfile(ROOT/'CREDITS.txt',DIST/'CREDITS.txt')
(DIST/'VALIDATION.json').write_text(json.dumps({'cartridge':report,'handling_and_pursuit':handling,'visual_fixture':scenery,'save_migration':migration,'region_transition':transition,'world_expansion':expansion},indent=2)+'\n')
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',13)
sheet=Image.new('RGB',(1024,1584),'#142b36');d=ImageDraw.Draw(sheet)
for i,(name,title) in enumerate((('race-city','CITY HIGHWAY / LIVE DASHBOARD'),('scenery-coast','PALM COAST'),('run-receipt','ITEMIZED TRAFFIC TICKET'),('garage','GARAGE AND TUNING'))):
    im=Image.open(validation/(name+'.png')).convert('RGB')
    assert im.size==(256,384),(name,im.size)
    x=(i%2)*512;y=(i//2)*792
    d.text((x+10,y+5),title,font=font,fill='#f4eac8')
    sheet.paste(im.resize((512,768),Image.Resampling.NEAREST),(x,y+24))
sheet.save(DIST/'Preview.png')
location_items=(('city','METRO DAWN'),('coast','PALM COAST'),('causeway','SKYWATER CAUSEWAY'),('pines','PINE RIDGE'),('canyon','REDROCK PASS'),('marina','MARINA NIGHTS'),('wetlands','MANGROVE RUN'),('groves','CITRUS PARKWAY'),('neon','NEON CURRENT'),('jungle','JADE CANOPY'),('swamp','GHOSTWATER BAYOU'),('aurora','AURORA REACH'),('salt','PRISM SALTWAY'),('motorcycle-wheelie','KESTREL / WHEELIE'))
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
for filename,bottom,items in (
    ('Turbo.png',True,(('turbo-guide','BUTTON-OPERATED TURBO GUIDE'),
                       ('turbo-empty','EMPTY RESERVE / RELEASE UP'),
                       ('turbo-recharging','SLOW RECOVERY'),
                       ('turbo-rearmed','BOOST REACTIVATED'))),
    ('Grounding.png',False,(('scenery-city','PROJECTED WALLS AND ROOFS'),
                           ('scenery-causeway','OPEN CAUSEWAY / ANCHORED LAMPS'),
                           ('grounded-boat','UPRIGHT BOAT PROFILE'),
                           ('grounded-pier','DOCK CONNECTED TO SHORE'))),
    ('Pursuit.png',True,(('handling-police-pressure','POLICE CLOSING IN'),
                         ('handling-stopped-ticket','CAUGHT / RUN EARNINGS'),
                         ('handling-quarter-combo','QUARTER-STEP COMBO'),
                         ('handling-banked-ticket','BANKED / CASH AWARDED'))),
    ('Progression.png',True,(('expansion-class-gate','FIVE VEHICLE CLASSES'),
                             ('expansion-style','PERMANENT VISUAL CUSTOMIZATION'),
                             ('expansion-contracts','OPTIONAL DISPATCH CONTRACTS'),
                             ('garage-parts','FOUR PERFORMANCE UPGRADE TRACKS'))),
):
    contact=Image.new('RGB',(1024,816),'#142b36');d=ImageDraw.Draw(contact)
    for i,(name,title) in enumerate(items):
        im=Image.open(validation/(name+'.png')).convert('RGB')
        assert im.size==(256,384),(name,im.size)
        x=(i%2)*512;y=(i//2)*408
        d.text((x+10,y+5),title,font=font,fill='#f4eac8')
        contact.paste(im.crop((0,192 if bottom else 0,256,384 if bottom else 192))
                      .resize((512,384),Image.Resampling.NEAREST),(x,y+24))
    contact.save(DIST/filename)
shutil.copyfile(validation/'highway-motion.gif',DIST/'highway-motion.gif')
shutil.copyfile(validation/'biome-transition.gif',DIST/'biome-transition.gif')
licenses=DIST/'licenses';licenses.mkdir(exist_ok=True)
for source in (ROOT/'licenses').glob('*.txt'):shutil.copyfile(source,licenses/source.name)
files=['Coastline.nds','PLAY.txt','CREDITS.txt','Preview.png','Locations.png','Vehicles.png','Dashboards.png','Interface.png','Turbo.png','Grounding.png','Pursuit.png','Progression.png','highway-motion.gif','biome-transition.gif','VALIDATION.json']+[f'licenses/{p.name}' for p in sorted(licenses.glob('*.txt'))]
(DIST/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256((DIST/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in files))
with zipfile.ZipFile(DIST/'Coastline-DS.zip','w',zipfile.ZIP_DEFLATED) as archive:
    for name in files+['SHA256SUMS.txt']:
        info=zipfile.ZipInfo('Coastline-DS/'+name,date_time=(2026,10,19,0,0,0))
        info.compress_type=zipfile.ZIP_DEFLATED
        archive.writestr(info,(DIST/name).read_bytes())
print('Packaged validated',rom.stat().st_size,'byte DS cartridge:',digest)
