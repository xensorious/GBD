"""Package the exact validated interface-studio cartridge and actual captures."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import zipfile
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--validation',type=Path,default=ROOT/'build'/'studio-validation')
a=p.parse_args();validation=a.validation
DIST=ROOT/'dist';DIST.mkdir(exist_ok=True)
rom=ROOT/'build'/'Coastline.nds';digest=hashlib.sha256(rom.read_bytes()).hexdigest()

def report(name):
    result=json.loads((validation/name).read_text())
    assert result.get('sha256',result.get('rom',{}).get('sha256'))==digest,('Stale validation report',name)
    return result

stock=report('results.json')
if not stock['native']:
    stock['native']=report('native-results.json')['native']
assert len(stock['native'])==14,'Run all fourteen native sanitizer suites.'
assert stock['battery_save_reload'] and stock['upgrade_purchased'],'Verify earned cash, an actual part purchase and battery reload.'
assert stock['long_drive_cadence']['updates']>0 and stock['long_drive_cadence']['hz']>=28
studio=report('studio-results.json')
assert studio['interfaces']==['Fusion','Pearl','Voltage','Alloy','Glacier','Bloom'] and len(studio['cases'])==6
assert all(case['fusion_navigation_passed'] for case in studio['cases']),'Verify Fusion grid, settings pages and fresh save reload.'
assert studio['garage_tabs']==['Showroom','Tuning','Style','Dealer']
assert studio['departure_interiors']==13 and studio['fresh_process_reload']
for case in studio['cases']:
    assert case['all_classes_reviewed'] and case['vehicles_reviewed']==48
    assert case['cosmetics_and_presets_passed'] and case['fresh_process_reload']
    assert case['serialized_save_bytes']==1000 and case['physical_save_bytes']==8192
legacy=report('migration-results.json');previous=report('studio-migration-results.json')
assert legacy['legacy_fields_preserved'] and legacy['fresh_process_reload']
for key in ('previous_v2_compatibility','previous_turbo_v2_compatibility','previous_handling_v2_compatibility'):
    case=legacy[key]
    assert case['legacy_fields_preserved'] and case['fresh_process_reload']
    assert case['serialized_save_bytes']==1000 and case['physical_save_bytes']==8192
assert previous['previous_v3_compatibility'] and previous['legacy_fields_preserved'] and previous['fresh_process_reload']
assert previous['serialized_save_bytes']==1000 and previous['physical_save_bytes']==8192

opening=report('opening-results.json')
assert opening['dual_screen_cinematic'] and opening['three_independent_profiles']
assert opening['fresh_process_reloads']==3 and opening['any_button_x_y_passed'] and opening['touch_ignored']
profiles=report('previous-profiles-results.json')
assert profiles['rich_cosmetics_and_presets_preserved']
assert profiles['all_progress_preserved'] and profiles['retired_interfaces_migrated_to_fusion']
assert profiles['profiles_imported']==3 and profiles['fresh_process_reloads']==3
v4=None
if (validation/'previous-v4-results.json').exists():
    v4=report('previous-v4-results.json')
    assert v4['profile_preserved_byte_for_byte'] and v4['fresh_process_reload']

extra=[]
for name in ('expansion-gate-4-results.json','expansion-style-results.json'):
    if (validation/name).exists():
        case=report(name);assert case['passed'] and case['fresh_process_reload'];extra.append(case)

premium=report('premium-results.json')
assert premium['purchases']==17 and premium['all_new_vehicles_driven'] and premium['motorcycle_wheelies']
assert premium['high_id_preset'] and premium['fresh_process_reload']
finishes=report('finishes-results.json')
assert len(finishes['body_paints'])==16 and len(finishes['livery_colors'])==12
assert finishes['replacement_liveries']==['Velocity','Contour']
assert finishes['independent_color_purchases'] and finishes['high_id_color_preset']
assert finishes['three_controls_pages'] and finishes['fresh_process_reload']

shutil.copyfile(rom,DIST/rom.name)
(DIST/'PLAY.txt').write_text((ROOT/'README.md').read_text().split('## Build and test')[0].rstrip()+'\n')
shutil.copyfile(ROOT/'CREDITS.txt',DIST/'CREDITS.txt')
(DIST/'VALIDATION.json').write_text(json.dumps({'cartridge':stock,'interface_studio':studio,'opening':opening,
    'finishes':finishes,'premium_roster':premium,'previous_three_profiles':profiles,'v1_v2_save_migration':legacy,'v3_save_migration':previous,'previous_v4_profile':v4,'dealer_and_style_regressions':extra,
    'scope':'Finish studio release: 16 body paints, 12 independent livery colors, two replacement patterns, high-bit presets, six interfaces, 48-model dealer tours, fourteen native sanitizer suites and v1/v2/v3/v5 save migrations. All 17 premium vehicles purchased and driven. Dual-screen opening, three independent profiles and fresh-process reloads passed. Physical Delta/iPhone/DS testing and broader world tours were not performed.'},indent=2)+'\n')
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',13)

def contact(filename,items,columns=2,screen='bottom'):
    dual=screen=='dual';cellh=792 if dual else 408
    sheet=Image.new('RGB',(512*columns,cellh*((len(items)+columns-1)//columns)),'#142b36');draw=ImageDraw.Draw(sheet)
    for i,(name,title) in enumerate(items):
        image=Image.open(validation/(name+'.png')).convert('RGB')
        assert image.size==(256,384),(name,image.size)
        if not dual:image=image.crop((0,192 if screen=='bottom' else 0,256,384 if screen=='bottom' else 192))
        x=i%columns*512;y=i//columns*cellh
        draw.text((x+10,y+5),title,font=font,fill='#eaf6f4')
        sheet.paste(image.resize((512,768 if dual else 384),Image.Resampling.NEAREST),(x,y+24))
    sheet.save(DIST/filename)

contact('Opening.png',[(name,title) for name,title in (('opening-title','DUAL-SCREEN OPENING'),('opening-slots-filled','THREE INDEPENDENT SAVE SLOTS'),('opening-slide-middle','SLIDE INTO THE MAIN MENU'),('opening-home','EXISTING MAIN MENU'))],screen='dual')
shutil.copyfile(validation/'opening-transition.gif',DIST/'Opening.gif')

contact('Preview.png',[
    ('ui-6-home','ALLOY / GRAY AND ORANGE'),('ui-7-home','GLACIER / COBALT GLASS'),
    ('ui-8-home','BLOOM / ROSE AND CREAM'),('finish-pattern-3','FINISH STUDIO / VELOCITY')],screen='dual')
contact('Interface.png',[(f'ui-{s}-{mode}',f'{name.upper()} / {mode.upper()}')
    for mode in ('home','settings') for s,name in enumerate(studio['interfaces'],3)],columns=3)
contact('Fusion.png',[(f'ui-3-{mode}',title) for mode,title in (
    ('home','FUSION / TILE MENU'),('settings','AUDIO / PERSONAL SETTINGS'),
    ('settings-display','DISPLAY / PERSONAL SETTINGS'),('settings-guide','GUIDE / CONTROLS LIBRARY'))])
contact('New-Interfaces.png',[(f'ui-{s}-{mode}',f'{name.upper()} / {mode.upper()}')
    for mode in ('home','settings','style') for s,name in ((6,'Alloy'),(7,'Glacier'),(8,'Bloom'))],columns=3)
contact('Garage.png',[(f'ui-{s}-{mode}',f'{name.upper()} / {mode.upper()}')
    for mode in ('showroom','tuning','style','dealer-4') for s,name in enumerate(studio['interfaces'],3)],columns=3)
locations=('METRO DAWN','PALM COAST','SKYWATER CAUSEWAY','PINE RIDGE','REDROCK PASS',
    'MARINA NIGHTS','MANGROVE RUN','CITRUS PARKWAY','NEON CURRENT','JADE CANOPY',
    'GHOSTWATER BAYOU','AURORA REACH','PRISM SALTWAY')
contact('Garage-Interiors.png',[(f'ui-3-room-{i:02d}',title+' / WORKSHOP') for i,title in enumerate(locations)],columns=3,screen='top')
contact('Locations.png',[(f'ui-3-location-{i:02d}',title+' / ROUTE PREVIEW') for i,title in enumerate(locations)],columns=3,screen='top')
contact('Progression.png',[
    ('ui-3-presets','PAID PRESET REFIT / SAVED SETUPS'),('ui-4-style','INSTALLED FINISH / RETURNING STYLES HALF PRICE'),
    ('ui-5-dealer-3','CLASS-BASED DEALER'),('ui-5-receipt','RUN STATEMENT / CASH AWARDED')])
contact('Premium-Vehicles.png',[(f"premium-{v['id']}-garage",f"{v['name'].upper()} / CLASS {v['class']} / ${v['price']}") for v in premium['vehicles']],columns=4,screen='dual')
contact('Premium-Driving.png',[(f"premium-{v['id']}-driving",f"{v['name'].upper()} / IN GAME") for v in premium['vehicles']],columns=4,screen='dual')
contact('Finishes.png',[
    ('finish-pattern-3','VELOCITY / SWEEPING STRIPE'),('finish-pattern-4','CONTOUR / BODY OUTLINES'),
    ('finish-signature','MAGENTA / ROSE GOLD / BATMOBILE'),('finish-preset','COLOR-AWARE SAVED PRESET')],screen='dual')
contact('Body-Paints.png',[(f'finish-paint-{i:02d}',name.upper()) for i,name in enumerate(finishes['body_paints'])],columns=4,screen='top')
contact('Livery-Colors.png',[(f'finish-color-{i:02d}',name.upper()) for i,name in enumerate(finishes['livery_colors'])],columns=4,screen='top')
# Remove only known superseded presentation files, never user files.
for name in ('Vehicles.png','Dashboards.png','Turbo.png','Grounding.png','Pursuit.png','highway-motion.gif','biome-transition.gif'):
    (DIST/name).unlink(missing_ok=True)
licenses=DIST/'licenses';licenses.mkdir(exist_ok=True)
for source in (ROOT/'licenses').glob('*.txt'):shutil.copyfile(source,licenses/source.name)
files=['Coastline.nds','PLAY.txt','CREDITS.txt','Preview.png','Opening.png','Opening.gif','Interface.png','Fusion.png','Garage.png',
    'Garage-Interiors.png','Locations.png','Progression.png','Premium-Vehicles.png','Premium-Driving.png',
    'New-Interfaces.png','Finishes.png','Body-Paints.png','Livery-Colors.png','VALIDATION.json']+[f'licenses/{p.name}' for p in sorted(licenses.glob('*.txt'))]
(DIST/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256((DIST/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in files))
archive_path=DIST/'Coastline-DS.zip'
with zipfile.ZipFile(archive_path,'w',zipfile.ZIP_DEFLATED) as archive:
    for name in files+['SHA256SUMS.txt']:
        info=zipfile.ZipInfo('Coastline-DS/'+name,date_time=(2026,10,19,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
        archive.writestr(info,(DIST/name).read_bytes())
with zipfile.ZipFile(archive_path) as archive:
    assert archive.testzip() is None
    assert archive.read('Coastline-DS/Coastline.nds')==rom.read_bytes()
print('Packaged validated',rom.stat().st_size,'byte DS cartridge:',digest)
