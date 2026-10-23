"""Package the exact validated interface-studio cartridge and actual captures."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import shutil
import zipfile
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--validation',type=Path,default=ROOT/'build'/'studio-validation')
p.add_argument('--allow-ui-preview-timing-regression','--allow-documented-timing-regression',dest='allow_ui_preview_timing_regression',action='store_true',help='Package a story review build with explicitly documented, bounded Free Roam timing failures; never mark the strict audit as passed.')
a=p.parse_args();validation=a.validation
DIST=ROOT/'dist';DIST.mkdir(exist_ok=True)
rom=ROOT/'build'/'Coastline.nds';digest=hashlib.sha256(rom.read_bytes()).hexdigest()
manifest=json.loads((ROOT/'build/story-manifest.json').read_text())

def report(name):
    result=json.loads((validation/name).read_text())
    assert result.get('sha256',result.get('rom_sha256',result.get('rom',{}).get('sha256')))==digest,('Stale validation report',name)
    return result

campaign=report('campaign-results.json')
assert campaign['chapters']==5 and campaign['events']==125 and not campaign['race_gameplay']
assert campaign['tabs']==['Route','Story','Rivals','Plans','Office']
assert campaign['dialogue_lines']==manifest['dialogue_lines'] and campaign['save_version']==7 and campaign['saved_progress']==6
assert len(campaign['earned_story_events'])==6 and campaign['fresh_process_reloads']==4
assert all(campaign[k] for k in ('button_only','touch_ignored','read_only_emulated_ram','three_profiles_isolated','replay_does_not_advance','abort_does_not_advance','locked_events_blocked','no_racing_cash_awarded','scene_autosaves_verified','shared_garage_return','existing_endless_launch','existing_city_launch'))
campaign_native=report('campaign-native-results.json')
assert campaign_native['checks']>=12000 and campaign_native['sanitizers_passed'] and campaign_native['all_scenes']==125
story_assets=report('story-assets-results.json');story_autosave=report('story-autosave-results.json')
assert story_assets['archive_matches_nitrofs'] and story_assets['scenes']==125 and story_assets['lines']==manifest['dialogue_lines'] and story_assets['portrait_alpha_masks_valid']
assert story_assets['resident_headroom_bytes']>0 and story_assets['itcm_headroom_bytes']>=0
quarter_count=manifest.get('quarter_portraits',0)
assert quarter_count in (0,142)
assert story_assets['backgrounds']==manifest['backgrounds']==54 and story_assets['portraits']==manifest['portraits']==76+quarter_count
if quarter_count:
    quarter_assets=report('quarter-assets-results.json');quarter_staging=report('quarter-staging-results.json')
    assert quarter_assets['legacy_background_payloads_preserved']==54 and quarter_assets['legacy_portrait_payloads_preserved']==76
    assert quarter_assets['independent_source_alpha_oracles_passed']==quarter_count and quarter_assets['quarter_maps_valid']
    assert quarter_assets['source_and_native_visual_reviews']==quarter_count and quarter_assets['alpha_extraction_uses_source_alpha_only']
    assert quarter_assets['encoded_story_events_preserved']==125 and quarter_assets['encoded_story_lines_preserved']==923
    assert quarter_assets['archive_portraits']==218 and quarter_assets['archive_assets']==272 and quarter_assets['maximum_portrait_id']==217
    assert quarter_assets['original_gallery_bindings_preserved'] and quarter_assets['story_speaker_ids_preserved']
    assert quarter_staging['sanitizers_passed'] and quarter_staging['complete_directional_assets']
    assert quarter_staging['scope']=='real_directional_art' and quarter_staging['authored_directional_gallery_views']==142
    assert quarter_staging['real_art_pair_configurations']==2145 and quarter_staging['compatible_floor_scenes']==8
    assert quarter_staging['pair_pixels_across_both_lcds'] and quarter_staging['speaker_swap_reloads']==0
    assert quarter_staging['speaker_sides_stable'] and quarter_staging['geometry_clips_no_opaque_pixels']
assert story_assets['ordinary_portraits']==manifest['ordinary_portraits']==57
assert story_assets['gallery_people']==manifest['gallery_count']==28 and story_assets['new_gallery_only_people']==14
assert story_assets['authored_portrait_references_valid'] and story_assets['draft_portraits_excluded_from_story']
assert story_assets['all_portraits_have_story_or_gallery_use']
assert story_assets['fullbody_portraits']==manifest['fullbody_portraits']==5
assert (story_assets['portrait_width'],story_assets['portrait_height'])==(manifest['portrait_width'],manifest['portrait_height'])==(256,264)
assert manifest['portrait_size']==manifest['portrait_width']
assert story_assets['one_event_new_backgrounds']>=19 and story_assets['new_place_events']>=62 and story_assets['all_portraits_used']
assert story_assets['no_large_atlas_fragments']
assert story_assets['authored_shots']==sum(len(e['shots']) for e in manifest['events'])
assert all(story_assets[k] for k in ('compiled_directions_match_source','subjects_in_final_frame','shot_local_companions'))
assert story_autosave['immediate_completion_save'] and story_autosave['fresh_process_reload']
stock=report('results.json')
if not stock['native']:
    stock['native']=report('native-results.json')['native']
assert len(stock['native'])==15,'Run all fifteen native sanitizer suites.'
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
city=report('city-results.json');current=report('previous-v6-results.json')
sunlight=city['sunlight']
shadow_qa=report('shadow-qa-results.json')
assert shadow_qa['native_views']['scenarios']==192 and shadow_qa['native_views']['pixel_checks']==18874368
assert len(shadow_qa['cases'])==3
performance=report('optimization-results.json') if (validation/'optimization-results.json').exists() else None
known_issues=[]
for case in shadow_qa['cases']:
    assert case['rom']['sha256']==digest and case['button_only'] and case['emulated_ram_read_only']
    pace=case['presentation_pacing']
    assert pace['samples']>200
    timing_passed=pace['worst_ms']<=50 and not pace['frames_over_50ms'] and pace['lcd_frame_gaps'].get('2',0)>=pace['samples']*.90
    if not timing_passed:
        assert a.allow_ui_preview_timing_regression,'Strict driving timing audit failed; no ordinary release may bypass it.'
        # An explicit review build exception, with the failed measurements retained.
        # Bound the exception: no longer stalls, stable median, and functional tests still required.
        assert pace['worst_ms']<=67 and pace['median_ms']<=34 and pace['p95_ms']<=50
        assert pace['frames_over_50ms']<pace['samples']*.005 and pace['lcd_frame_gaps'].get('2',0)>=pace['samples']*.85
        known_issues.append({'mode':'Free Roam','model':case['model'],'strict_timing_audit_passed':False,'measurements':pace,
            'note':'Story review build: rare frame delays remain in the extended city route. Functional, save, shadow-position and UI checks pass; the strict driving timing check fails.'})
    assert all(probe['missing_shadow']<=8 and probe['unexplained_shadow']<=8 for probe in case['shadow_probes'])
if performance:
    assert not known_issues and performance['strict_routes_passed']
    assert performance['native_equivalence']['pixels']==24379392 and performance['native_equivalence']['simulation_and_save_bytes_match']
    assert performance['vehicle_textures']['all_pixels_match_previous'] and performance['vehicle_textures']['source_pixels_checked']==82944
    assert performance['vehicle_textures']['rom']['sha256']==digest
    assert performance['actual_ds_frames']['all_match_previous'] and performance['actual_ds_frames']['pixels']==245760
    assert performance['pose_uploads']['optimized']==0
assert all(sunlight[key] for key in ('world_space','actual_player_full_sun','actual_player_partial_shade','actual_player_full_shade'))
assert sunlight['max_cached_casters']<192 and sunlight['partial_vehicle_frames']>0
assert any('145152 independent 3D ray/roof cases' in result and '9216 cache-boundary receiver checks pass' in result for result in stock['native'])
sunlight_comparison=report('sunlight-comparison-results.json')
assert sunlight_comparison['previous_rom_sha256']=='1e6eba60569504dd69ba270c60473fab60226ee6dcbee521b9d7cb5fe5bb7a1c'
for name,expected in sunlight_comparison['files'].items():
    assert hashlib.sha256((validation/name).read_bytes()).hexdigest()==expected,('Stale sunlight capture',name)
assert city['actual_city_route_completed'] and city['actual_bridge_crossing'] and city['city_hz']>=25
assert city['button_only_mode_selector'] and city['touch_ignored'] and city['shared_owned_vehicle_and_cosmetics']
assert city['pause_freezes_city'] and city['shared_banked_cash'] and city['fresh_process_reload']
assert current['profile_fields_preserved'] and current['version_migrated']==7 and current['fresh_process_reload']
assert city['heading_locked_camera'] and city['actual_full_circle_and_heading_wrap']
assert city['observed_headings']>=32 and city['max_submitted_quads']<900
assert not city['ds_geometry_overflow'] and not city['ds_color_underflow']
assert city['texture_banks_restored_on_reentry_and_highway']
assert city['junction_sway_exercised'] and city['road_stability']['passed']
assert city['road_stability']['frames']>150 and city['road_stability']['interior_asphalt_pixels']>20000
assert city['road_stability']['observed_material_pixels']>1000
assert any('1161216 independent model-sized contact cases' in result for result in stock['native'])
mesh=report('city-mesh-results.json')
assert mesh['orthographic_facades']==16 and mesh['roof_materials']==16
assert mesh['all_texture_regions_valid'] and mesh['source_hash_matches']
assert mesh['native_rasterizer_excluded_from_rom'] and mesh['retired_flat_building_bank_excluded_from_rom']
assert any('256 heading-locked projections, rotated ground invariance at 24 cardinal views' in result for result in stock['native'])
refinement_report=report('free-roam-refinement-results.json')
assert refinement_report['previous_rom_sha256']=='3d5ed697078702c74e9a37dffb832e2f07efb676e353ba46001062404df3d6c7'
for name,expected in refinement_report['files'].items():
    assert hashlib.sha256((validation/name).read_bytes()).hexdigest()==expected,('Stale refinement capture',name)

masks=report('material-mask-results.json')
assert masks['models_reviewed']==48 and masks['poses_reviewed']==240 and masks['fitted_liveries']==960
assert masks['pacer_lenses_follow_nonrectangular_outlines'] and masks['packed_roles_follow_artwork_transform']
assert masks['source_art_hashes_match'] and masks['decals_only_on_painted_pixels']
comparison=report('paint-comparison-results.json')
assert hashlib.sha256((validation/'paint-before.ppm').read_bytes()).hexdigest()==comparison['before_ppm_sha256']
assert hashlib.sha256((validation/'paint-review.ppm').read_bytes()).hexdigest()==comparison['after_ppm_sha256']

world_art=report('openworld-art-results.json')
assert world_art['models']==48 and world_art['minimum_palette_colors']>32
assert world_art['reviewed_body_and_protected_landmarks_pass'] and world_art['native_oracles_excluded_from_rom']
assert any('world-fixed/clipped lane paint across 288 camera shifts' in result for result in stock['native'])
world_comparison=report('openworld-comparison-results.json')
for name,expected in world_comparison['files'].items():
    assert hashlib.sha256((validation/name).read_bytes()).hexdigest()==expected,('Stale city preview',name)

environment=report('city-environment-results.json')
assert environment['buildings']==16 and environment['garden_objects']==16 and environment['wind_poses']==8
assert environment['lawn_seams_match'] and environment['sources_match'] and environment['building_silhouettes_unclipped']
assert any('16 district-weighted buildings, rooted wind poses, pavement-safe gardens' in result for result in stock['native'])
scenery_comparison=report('scenery-comparison-results.json')
for name,expected in scenery_comparison['files'].items():
    assert hashlib.sha256((validation/name).read_bytes()).hexdigest()==expected,('Stale scenery preview',name)

shutil.copyfile(rom,DIST/rom.name)
(DIST/'PLAY.txt').write_text((ROOT/'README.md').read_text().split('## Build and test')[0].rstrip()+'\n')
if known_issues:
    with (DIST/'PLAY.txt').open('a') as f:
        f.write('\nKNOWN VALIDATION ISSUE\nThe extended Free Roam route fails the strict driving timing limit.\n')
        for issue in known_issues:
            m=issue['measurements'];f.write(f"Model {issue['model']}: {m['frames_over_50ms']} of {m['samples']} measured frame intervals exceed 50 ms; worst {m['worst_ms']:.1f} ms, median {m['median_ms']:.1f} ms.\n")
        f.write('Career has playable story scenes; race gameplay is not implemented.\n')
shutil.copyfile(ROOT/'CREDITS.txt',DIST/'CREDITS.txt')
shutil.copyfile(validation/'shadow-qa-results.json',DIST/'SHADOW-AUDIT.json')
(DIST/'VALIDATION.json').write_text(json.dumps({'known_issues':known_issues,'strict_driving_timing_passed':not known_issues,'optimization':performance,'cartridge':stock,'interface_studio':studio,'opening':opening,
    'afternoon_sunlight':sunlight,'sunlight_comparison':sunlight_comparison,
    'shadow_qa':shadow_qa,'career_preview':campaign,'career_native':campaign_native,'story_assets':story_assets,'story_autosave':story_autosave,
    'quarter_assets':quarter_assets if quarter_count else None,'quarter_staging':quarter_staging if quarter_count else None,
    'free_roam_refinement':refinement_report,'rotating_city_mesh':mesh,'city_environment':environment,'scenery_comparison':scenery_comparison,'openworld_art':world_art,'openworld_comparison':world_comparison,'pixel_material_masks':masks,'paint_comparison':comparison,'free_roam':city,'previous_v6_profile':current,'finishes':finishes,'premium_roster':premium,'previous_three_profiles':profiles,'v1_v2_save_migration':legacy,'v3_save_migration':previous,'previous_v4_profile':v4,'dealer_and_style_regressions':extra,
    'scope':'Fixed northwest afternoon sunlight casts southeast roof hulls and alpha canopy silhouettes. The shared world-space cache shades player and NPC vehicle vertices progressively; uniform-light vehicles retain one quad, and existing material art is preserved. Spatial buckets and conservative scenery culling keep the DS workload bounded. Independent ray/roof tests cover 145,152 cases; all 1024 chunks, canopy holes, uncached shadow unions and cache boundaries are checked. Actual button-driven captures verify full sunlight, partial shade and full shade. Free Roam refinement: ground layers compose in a separate far-depth band; direct Q4 vertices avoid the old +/-512px street-corner wrap; the view is pulled back 12.5% with fractional camera translation. Smaller per-model sprites share oriented collision/headway dimensions, verified with 1,161,216 independent polygon-clipping contact cases. Dynamic player poses are prepared before a short bank-D-only upload. Actual road-pixel inspection covers hundreds of frames including a junction sway. Heading-locked Free Roam camera using Nintendo DS hardware 3D: sixteen original orthographic facade textures, real walls and pitched/flat roofs, dimensional bridge rails and streetlights, world-fixed roads and ground, overhead canopies/gardens/boats and the existing material-protected vehicle paintings. Native checks cover all 256 headings, vertical height, world-translation cancellation and ground overlap at 24 cardinal views, plus geometry, assets and gameplay. Actual button-only cartridge tests turn through a full circle/heading wrap, capture diagonal junction turns, cross the canal, check DS geometry/underflow flags and performance, re-enter the city, restore the bitmap highway, and reload the shared battery save. Functional checks for both playable driving modes, the playable Career story scenes, six interfaces, purchases, vehicle finishes, profiles and save migrations pass on this exact ROM. Strict driving timing is reported separately and may fail in explicitly labelled UI review builds. Native district views are explicitly labelled diagnostic renders. Physical Delta/iPhone/DS testing was not performed.'},indent=2)+'\n')
shutil.copyfile(ROOT/'CAREER-DRAFT.md',DIST/'CAREER-DRAFT.md')
shutil.copyfile(ROOT/'STORY-REVIEW.md',DIST/'STORY-REVIEW.md')
shutil.copyfile(ROOT/'story/rising_tide.txt',DIST/'STORY-SCRIPT.txt')
shutil.copyfile(ROOT/'art/story/cast.png',DIST/'Story-Cast.png')
shutil.copyfile(validation/'campaign-results.json',DIST/'CAREER-VALIDATION.json')
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
contact('Career.png',[
 ('story-d01-0','D.01 / the jack behind the first joke'),('story-d01-6','D.01 / conversation at the club meet'),
 ('story-d05-0','D.05 / a meal at Eli\'s diner'),('story-d06-4','Exhibition setback / narrated montage'),
 ('story-b15-0','The offer / a visiting Voss at Sunline'),('story-a14-0','Crew meeting / people with different priorities'),
 ('story-s25-12','The final chapter / a door left open'),('story-six-events-earned','Six scenes earned / saved map progression')],screen='dual')
contact('Story-Progress.png',[
 ('story-d01-clear','First scene completed / autosaved'),('story-d06-clear','Training block unlocked'),
 ('story-profile-1-reload','Fresh process / original profile continues'),('story-profile-2-reload','Second profile / independent story record'),
 ('story-shared-garage','Existing shared garage'),('story-return-city','Back to driving / released art scratch memory')],screen='dual')
contact('Story-Direction.png',[
 ('story-d09-3','D.09 / the kitchen table'),('story-d24-3','D.24 / the family photograph'),
 ('story-d19-0','D.19 / reference marker'),('story-d19-2','D.19 / pan to the corner exit'),
 ('story-c07-0','C.07 / rain on the waterfront'),('story-c07-3','C.07 / shelter under the borrowed awning'),
 ('story-b20-3','B.20 / the workshop Voss lost'),('story-b20-6','B.20 / a less guarded answer'),
 ('story-a21-1','A.21 / commercial contract'),('story-a21-4','A.21 / independent sporting application'),
 ('story-s20-0','S.20 / readable Cup terms'),('story-s25-12','S.25 / another chair by the door')],screen='dual')
contact('Story-Life.png',[
 ('story-d09-6','Nia / a kitchen two streets from Sunline'),('story-d10-3','Mina / notes in her bedroom'),
 ('story-d17-6','Jules / the invoices behind the jokes'),('story-d24-6','Calder and Nia / the lighthouse walk'),
 ('story-c10-0','Imani / washing the shift away'),('story-c19-7','Imani / an ordinary Thursday'),
 ('story-b19-0','Mina / work with her own name'),('story-b20-6','Voss / the old workshop'),
 ('story-a06-0','A washroom mirror / rehearsing belonging'),('story-a13-0','The sky lounge / another kind of security'),
 ('story-s04-0','Mina / her own small exhibition'),('story-s25-13','Eli and Vale / one more person to welcome')],screen='dual')

# These two sheets show encoded assets, not simulated scenes or emulator output.
catalog=json.loads((ROOT/'story/cast.json').read_text())
places=json.loads((ROOT/'story/staging.json').read_text())['backgrounds']
archive=(ROOT/'build/nitro/story.bin').read_bytes();table=struct.unpack_from('<I',archive,16)[0]
def indexed_art(asset):
    offset,w,h,_,kind=struct.unpack_from('<IHHII',archive,table+asset*16)
    palette=struct.unpack_from('<256H',archive,offset);pixels=archive[offset+512:offset+512+w*h]
    image=Image.new('RGBA',(w,h))
    image.putdata([((palette[v]&31)*255//31,(palette[v]>>5&31)*255//31,(palette[v]>>10&31)*255//31,0 if kind and not v else 255) for v in pixels])
    return image
sheet=Image.new('RGB',(1536,24+((len(places)+5)//6)*126),'#142b36');draw=ImageDraw.Draw(sheet)
draw.text((8,5),'INDEXED BACKGROUND ART / full panoramic assets / not emulator screenshots',font=font,fill='white')
for i,place in enumerate(places):
    x=i%6*256;y=24+i//6*126;sheet.paste(indexed_art(i).resize((256,108),Image.Resampling.NEAREST),(x,y))
    draw.text((x+4,y+110),place.get('location',place['name']),font=font,fill='#d7e7e4')
sheet.save(DIST/'Story-Locations.png')
portrait_width=manifest['portrait_width'];portrait_height=manifest['portrait_height'];portrait_row=portrait_height+30
portrait_map=manifest['portrait_map'];fullbody_map=manifest['fullbody_map'];cast_gallery=manifest['gallery']
assert len(portrait_map)==len(fullbody_map)==len(catalog['poses'])==5
assert manifest['people']==len(catalog['people'])==14 and all(len(row)==14 for row in portrait_map+fullbody_map)
ordinary_ids={value for row in portrait_map for value in row}
fullbody_ids={value for row in fullbody_map for value in row if value>=0}
assert ordinary_ids==set(range(manifest['ordinary_portraits'])) and len(ordinary_ids)==57
assert len(fullbody_ids)==manifest['fullbody_portraits']==5
assert len(cast_gallery)==manifest['gallery_count']==28
assert [person['id'] for person in cast_gallery[:14]]==[person[0] for person in catalog['people']]
assert all(not person['draft'] for person in cast_gallery[:14]) and all(person['draft'] for person in cast_gallery[14:])
assert all(len(person['portraits'])==len(person['full_body'])==5 for person in cast_gallery)
draft_ids={portrait for person in cast_gallery[14:] for portrait in person['portraits']}
assert len(draft_ids)==14 and all(len(set(person['portraits']))==1 and all(person['full_body']) for person in cast_gallery[14:])
assert not ordinary_ids&fullbody_ids and not draft_ids&(ordinary_ids|fullbody_ids)
quarter_entries=manifest.get('quarter_entries',[])
quarter_ids={entry['portrait'] for entry in quarter_entries}
assert len(quarter_entries)==quarter_count and quarter_ids==set(range(76,76+quarter_count))
assert ordinary_ids|fullbody_ids|draft_ids==set(range(76))
assert ordinary_ids|fullbody_ids|draft_ids|quarter_ids==set(range(manifest['portraits']))
nia=next(i for i,person in enumerate(catalog['people']) if person[0]=='NIA')
assert all(value>=0 if person==nia else value==-1 for row in fullbody_map for person,value in enumerate(row))
sheet=Image.new('RGB',(8*portrait_width,24+((manifest['portraits']+7)//8)*portrait_row),'#142b36');draw=ImageDraw.Draw(sheet)
draw.text((8,5),f"INDEXED CHARACTER ART / {len(ordinary_ids)} upper-body + {len(fullbody_ids)} Nia full-body + {len(draft_ids)} concept studies + {quarter_count} directional full bodies / not emulator screenshots",font=font,fill='white');seen=set()
for mapping,fullbody in ((portrait_map,False),(fullbody_map,True)):
    for pose,row in enumerate(mapping):
        for person,portrait in enumerate(row):
            if portrait<0 or portrait in seen:continue
            seen.add(portrait);x=portrait%8*portrait_width;y=24+portrait//8*portrait_row;im=indexed_art(manifest['backgrounds']+portrait)
            assert im.size==(portrait_width,portrait_height)
            sheet.paste(im,(x,y),im);draw.text((x+3,y+portrait_height+1),catalog['people'][person][1],font=font,fill='white')
            draw.text((x+3,y+portrait_height+14),catalog['poses'][pose].title()+(' / Full body' if fullbody else ''),font=font,fill='#9cbcb9')
for person in cast_gallery[14:]:
    portrait=person['portraits'][0]
    assert portrait not in seen
    seen.add(portrait);x=portrait%8*portrait_width;y=24+portrait//8*portrait_row;im=indexed_art(manifest['backgrounds']+portrait)
    assert im.size==(portrait_width,portrait_height)
    sheet.paste(im,(x,y),im);draw.text((x+3,y+portrait_height+1),person['name']+' / Concept',font=font,fill='white')
    draw.text((x+3,y+portrait_height+14),person['style'],font=font,fill='#9cbcb9')
for entry in quarter_entries:
    portrait=entry['portrait'];assert portrait not in seen
    seen.add(portrait);x=portrait%8*portrait_width;y=24+portrait//8*portrait_row;im=indexed_art(manifest['backgrounds']+portrait)
    assert im.size==(portrait_width,portrait_height)
    person=next(person for person in cast_gallery if person['id']==entry['character'])
    sheet.paste(im,(x,y),im);draw.text((x+3,y+portrait_height+1),person['name']+' / '+entry['pose'].title(),font=font,fill='white')
    draw.text((x+3,y+portrait_height+14),'Three-quarter / '+entry['facing'].lower(),font=font,fill='#9cbcb9')
assert seen==set(range(manifest['portraits']))
sheet.save(DIST/'Story-Wardrobes.png')
shutil.copyfile(validation/'opening-transition.gif',DIST/'Opening.gif')

contact('Preview.png',[
    ('city-mode-selected','DRIVE / TWO GAME MODES'),('city-route-1','FREE ROAM / NEIGHBORHOOD STREETS'),
    ('city-crossing','FREE ROAM / RIVER CROSSING'),('race-city','ENDLESS DRIVE / ORIGINAL HIGHWAY')],screen='dual')
contact('Free-Roam.png',[
    ('city-mode-select','ENDLESS DRIVE / FREE ROAM'),('city-spawn','YOUR OWN GARAGE VEHICLE'),
    ('city-route-1','NEIGHBORHOOD JUNCTIONS'),('city-crossing','CONNECTED HIGHWAY BRIDGE'),
    ('city-route-4','GLASS QUAY / CONTINUE EXPLORING'),('city-receipt','SHARED EARNINGS / BANKED RUN')],columns=3,screen='dual')
shutil.copyfile(validation/'Free-Roam.gif',DIST/'Free-Roam.gif')
contact('Rotating-Camera.png',[
    ('city-turn-east','DIAGONAL TURN / DS EMULATOR'),('city-turn-south','JUNCTION / DS EMULATOR'),
    ('city-circle-048','VIEW 1 / DS EMULATOR'),('city-circle-112','VIEW 2 / DS EMULATOR'),
    ('city-circle-176','VIEW 3 / DS EMULATOR'),('city-circle-240','VIEW 4 / DS EMULATOR')],columns=3,screen='top')
contact('Sunlight.png',[
    ('city-sun-sunlit','SUNLIGHT / DS EMULATOR'),('city-sun-partial','PARTIAL SHADE / DS EMULATOR'),
    ('city-sun-shade','FULL SHADE / DS EMULATOR'),('city-turn-east','DIRECTIONAL BUILDING SHADOWS'),
    ('city-route-1','SHADED NEIGHBORHOOD'),('city-crossing','AFTERNOON / CANAL CROSSING')],columns=3,screen='top')
contact('Shadow-Audit.png',[
    ('audit-stop-00','DOWNTOWN / PAUSED DS VIEW'),('audit-stop-01','TURNING VIEW / PAUSED DS VIEW'),
    ('city-route-4','GLASS QUAY / DRIVING DS VIEW'),('audit-stop-04','PALMETTO HEIGHTS / PAUSED DS VIEW'),
    ('audit-stop-08','CANAL GARDENS / PAUSED DS VIEW'),('audit-stop-15','OCEAN BOULEVARD / PAUSED DS VIEW')],columns=3,screen='top')
sun_comparison=Image.new('RGB',(1024,1224),'#18303c');sun_draw=ImageDraw.Draw(sun_comparison)
for row,(name,title) in enumerate((('city-spawn','OCEAN BOULEVARD'),('city-turn-east','JUNCTION TURN'),('city-route-2','DOWNTOWN WEST'))):
    for column,prefix in enumerate(('sun-before-','')):
        x=column*512;y=row*408
        sun_draw.text((x+8,y+5),('PREVIOUS' if column==0 else 'AFTERNOON SUN')+' / '+title+' / DS EMULATOR',font=font,fill='#eaf6f4')
        view=Image.open(validation/(prefix+name+'.png')).convert('RGB').crop((0,0,256,192))
        sun_comparison.paste(view.resize((512,384),Image.Resampling.NEAREST),(x,y+24))
sun_comparison.save(DIST/'Sunlight-Comparison.png')
refinement=Image.new('RGB',(1024,1224),'#18303c');refinement_draw=ImageDraw.Draw(refinement)
for row,(name,title) in enumerate((('city-spawn','LANE CLEARANCE'),('city-turn-east','TURNING VIEW'),('city-crossing','HIGHWAY / CANAL'))):
    for column,prefix in enumerate(('polish-before-','')):
        x=column*512;y=row*408
        refinement_draw.text((x+8,y+5),('PREVIOUS' if column==0 else 'REFINED')+' / '+title+' / DS EMULATOR',font=font,fill='#eaf6f4')
        view=Image.open(validation/(prefix+name+'.png')).convert('RGB').crop((0,0,256,192))
        refinement.paste(view.resize((512,384),Image.Resampling.NEAREST),(x,y+24))
refinement.save(DIST/'Free-Roam-Refinement.png')
gallery=Image.open(validation/'overhead-vehicles.ppm').convert('RGB')
assert gallery.size==(512,384)
gallery.resize((1024,768),Image.Resampling.NEAREST).save(DIST/'Overhead-Vehicles.png')
finishes_gallery=Image.open(validation/'vehicle-finishes.ppm')
finishes_gallery.save(DIST/'Vehicle-Finishes.png')
vehicle_details=Image.new('RGB',(480,576))
for row,model in enumerate((31,32,36,42,43,47)):
    vehicle_details.paste(finishes_gallery.crop((0,model*96,480,(model+1)*96)),(0,row*96))
vehicle_details.resize((960,1152),Image.Resampling.NEAREST).save(DIST/'Vehicle-Details.png')
# Pixel-accurate review strips captured by the cartridge's native renderer.
paint_review=Image.open(validation/'paint-review.ppm').convert('RGB')
assert paint_review.size==(384,4608)
paint_review.save(DIST/'Paint-Review.png')
vehicle_block=(ROOT/'src'/'game.c').read_text().split('const VehicleDef vehicles[VEHICLE_COUNT] = {',1)[1].split('};',1)[0]
vehicle_names=re.findall(r'\{\s*"([^"]+)"',vehicle_block)
assert len(vehicle_names)==48
for page in range(4):
    sheet=Image.new('RGB',(768,2208),'#18303c');draw=ImageDraw.Draw(sheet)
    for col,label in enumerate(('FACTORY','BLACK','GRAPHITE','TWIN','TWO TONE','VELOCITY','CONTOUR','MASK')):
        draw.text((col*96+4,8),label,font=font,fill='#eaf6f4')
    for row,model in enumerate(range(page*12,page*12+12)):
        yy=36+row*180
        draw.text((6,yy),f'{model:02d} / {vehicle_names[model]}',font=font,fill='#eaf6f4')
        strip=paint_review.crop((0,model*96+18,384,model*96+90))
        sheet.paste(strip.resize((768,144),Image.Resampling.NEAREST),(0,yy+22))
    sheet.save(DIST/f'Paint-Review-{page+1}.png')
previous=Image.open(validation/'paint-before.ppm').convert('RGB');assert previous.size==(288,144)
models=(0,6,14,16,24,38)
sheet=Image.new('RGB',(672,1424),'#18303c');draw=ImageDraw.Draw(sheet)
for col,label in enumerate(('FACTORY','PREVIOUS BLACK','FIXED BLACK','GRAPHITE')):
    draw.text((100+col*144,8),label,font=font,fill='#eaf6f4')
for row,model in enumerate(models):
    yy=32+row*232
    draw.text((5,yy+88),f'{model:02d}\n{vehicle_names[model]}',font=font,fill='#eaf6f4')
    images=(paint_review.crop((0,model*96+18,48,model*96+90)),
            previous.crop((row*48,72,row*48+48,144)),
            paint_review.crop((48,model*96+18,96,model*96+90)),
            paint_review.crop((96,model*96+18,144,model*96+90)))
    for col,frame in enumerate(images):sheet.paste(frame.resize((144,216),Image.Resampling.NEAREST),(96+col*144,yy))
sheet.save(DIST/'Paint-Fix-Comparison.png')

# Actual-cartridge scenes and separately labelled native vehicle-sheet comparison.
world_sheet=Image.new('RGB',(1024,1264),'#18303c');draw=ImageDraw.Draw(world_sheet)
for column,(name,label) in enumerate((('city-before.png','BEFORE / DS EMULATOR'),('city-spawn.png','UPDATED / DS EMULATOR'))):
    draw.text((column*512+10,7),label,font=font,fill='#eaf6f4')
    view=Image.open(validation/name).convert('RGB').crop((0,0,256,192))
    world_sheet.paste(view.resize((512,384),Image.Resampling.NEAREST),(column*512,28))
for column,(name,label) in enumerate((('overhead-before.ppm','BEFORE / NATIVE VEHICLE RENDERER'),('overhead-vehicles.ppm','UPDATED / NATIVE VEHICLE RENDERER'))):
    draw.text((column*512+10,428),label,font=font,fill='#eaf6f4')
    view=Image.open(validation/name).convert('RGB');assert view.size==(512,384)
    world_sheet.paste(view,(column*512,456))
for column,(name,label) in enumerate((('city-route-1.png','UPDATED / NEIGHBORHOOD JUNCTION'),('city-crossing.png','UPDATED / MARINA CROSSING'))):
    draw.text((column*512+10,848),label,font=font,fill='#eaf6f4')
    view=Image.open(validation/name).convert('RGB').crop((0,0,256,192))
    world_sheet.paste(view.resize((512,384),Image.Resampling.NEAREST),(column*512,872))
world_sheet.save(DIST/'Open-World-Upgrade.png')
world_finishes=Image.open(validation/'overhead-review.ppm').convert('RGB');assert world_finishes.size==(384,4608)
world_finishes.save(DIST/'Overhead-Finishes.png')
for page in range(4):
    sheet=Image.new('RGB',(768,2208),'#18303c');draw=ImageDraw.Draw(sheet)
    for col,label in enumerate(('FACTORY','BLACK','GRAPHITE','WHITE','TWIN','TWO TONE','VELOCITY','CONTOUR')):
        draw.text((col*96+3,8),label,font=font,fill='#eaf6f4')
    for row,model in enumerate(range(page*12,page*12+12)):
        yy=36+row*180;draw.text((6,yy),f'{model:02d} / {vehicle_names[model]}',font=font,fill='#eaf6f4')
        strip=world_finishes.crop((0,model*96+18,384,model*96+90))
        sheet.paste(strip.resize((768,144),Image.Resampling.NEAREST),(0,yy+22))
    sheet.save(DIST/f'Overhead-Finishes-{page+1}.png')

# Direct before/after pairs use real DS framebuffer captures at the same tour points.
scenery_sheet=Image.new('RGB',(1024,1224),'#18303c');draw=ImageDraw.Draw(scenery_sheet)
for row,(before,after,title) in enumerate((('scenery-before-spawn','city-spawn','DOWNTOWN STREET'),('scenery-before-junction','city-route-1','NEIGHBORHOOD JUNCTION'),('scenery-before-bridge','city-crossing','CANAL CROSSING'))):
    for column,name in enumerate((before,after)):
        x=column*512;y=row*408;draw.text((x+8,y+5),('BEFORE' if column==0 else 'UPDATED')+' / '+title+' / DS EMULATOR',font=font,fill='#eaf6f4')
        view=Image.open(validation/(name+'.png')).convert('RGB').crop((0,0,256,192))
        scenery_sheet.paste(view.resize((512,384),Image.Resampling.NEAREST),(x,y+24))
scenery_sheet.save(DIST/'Living-City.png')
districts=Image.new('RGB',(1536,816),'#18303c');draw=ImageDraw.Draw(districts)
for index,title in enumerate(('OCEAN BOULEVARD','DOWNTOWN WEST','GLASS QUAY','CANAL GARDENS','PALMETTO HEIGHTS','MARINA BRIDGE')):
    x=index%3*512;y=index//3*408
    draw.text((x+8,y+5),title+' / NATIVE SCENE REVIEW',font=font,fill='#eaf6f4')
    view=Image.open(validation/f'district-{index}-native.ppm').convert('RGB').crop((0,0,256,192))
    districts.paste(view.resize((512,384),Image.Resampling.NEAREST),(x,y+24))
districts.save(DIST/'Districts.png')
# Show the exact indexed RGB555 atlas used by the GPU, not unquantized art.
import array
import sys
indices=(ROOT/'build/city_mesh_idx.bin').read_bytes()
palette=array.array('H',(ROOT/'build/city_mesh_palette.bin').read_bytes())
if sys.byteorder!='little':palette.byteswap()
atlas=Image.new('RGBA',(256,512))
atlas.putdata([((palette[c]&31)*255//31,((palette[c]>>5)&31)*255//31,((palette[c]>>10)&31)*255//31,255) if c else (0,0,0,0) for c in indices])
assets=Image.new('RGB',(1024,2112),'#18303c');draw=ImageDraw.Draw(assets)
draw.text((10,8),'ROTATING CITY / GPU WALLS, ROOFS, FOLIAGE AND GROUND / PACKED RGB555',font=font,fill='#eaf6f4')
scaled=atlas.resize((1024,2048),Image.Resampling.NEAREST)
assets.paste(scaled,(0,32),scaled)
draw.text((10,2090),'Straight wall elevations and roof materials are applied to real 3D geometry.',font=font,fill='#eaf6f4')
assets.save(DIST/'Coastal-Scenery.png')

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
files=['Story-Life.png','Story-Locations.png','Story-Wardrobes.png','Story-Direction.png','STORY-REVIEW.md','Story-Progress.png','Story-Cast.png','STORY-SCRIPT.txt','Career.png','CAREER-DRAFT.md','CAREER-VALIDATION.json','Coastline.nds','PLAY.txt','CREDITS.txt','Preview.png','Sunlight.png','Sunlight-Comparison.png','Shadow-Audit.png','SHADOW-AUDIT.json','Opening.png','Opening.gif','Interface.png','Fusion.png','Garage.png',
    'Garage-Interiors.png','Locations.png','Progression.png','Premium-Vehicles.png','Premium-Driving.png',
    'New-Interfaces.png','Finishes.png','Body-Paints.png','Livery-Colors.png',
    'Free-Roam.png','Free-Roam.gif','Rotating-Camera.png','Free-Roam-Refinement.png','Living-City.png','Districts.png','Coastal-Scenery.png','Open-World-Upgrade.png','Overhead-Finishes.png','Overhead-Finishes-1.png','Overhead-Finishes-2.png','Overhead-Finishes-3.png','Overhead-Finishes-4.png','Overhead-Vehicles.png','Vehicle-Finishes.png','Vehicle-Details.png','Paint-Fix-Comparison.png','Paint-Review.png',
    'Paint-Review-1.png','Paint-Review-2.png','Paint-Review-3.png','Paint-Review-4.png','VALIDATION.json']+[f'licenses/{p.name}' for p in sorted(licenses.glob('*.txt'))]
if performance:
    shutil.copyfile(ROOT/'PERFORMANCE.md',DIST/'PERFORMANCE.md')
    (DIST/'PERFORMANCE.json').write_text(json.dumps(performance,indent=2)+'\n')
    files+=['PERFORMANCE.md','PERFORMANCE.json']
(DIST/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256((DIST/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in files))
archive_path=DIST/'Coastline-DS.zip'
with zipfile.ZipFile(archive_path,'w',zipfile.ZIP_DEFLATED) as archive:
    for name in files+['SHA256SUMS.txt']:
        info=zipfile.ZipInfo('Coastline-DS/'+name,date_time=(2026,10,19,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
        archive.writestr(info,(DIST/name).read_bytes())
with zipfile.ZipFile(archive_path) as archive:
    assert archive.testzip() is None
    assert archive.read('Coastline-DS/Coastline.nds')==rom.read_bytes()
print('Packaged story review cartridge with documented timing failure:' if known_issues else 'Packaged validated cartridge:',rom.stat().st_size,'bytes;',digest)
