"""Read-only fixed-width ARM EABI view of Coastline game.h.

No write-back mechanism is provided. Emulator assertions observe the actual
cartridge's Game symbol, and every action is delivered through joypad input.
"""
import ctypes as C
import subprocess
from pathlib import Path

U8,U16,U32,I16,I32=C.c_uint8,C.c_uint16,C.c_uint32,C.c_int16,C.c_int32
VEHICLE_COUNT,LOCATION_COUNT,SAVE_DISK_SIZE=31,13,500
class LegacySave(C.LittleEndianStructure):
    _fields_=[(n,U32) for n in (
        'magic','version','checksum','cash','owned','unlocked_locations','paints',
        'best_score','best_distance','best_near','best_drift','best_speed_hold',
        'best_wheelie','best_coins','total_runs','total_distance','total_cash','seed')]+[
        ('selected_vehicle',U8),('selected_location',U8),('paint',U8*6),
        ('engine',U8*6),('grip',U8*6),('armor',U8*6),('reserved',U8*2)]
class V2Save(C.LittleEndianStructure):
    _fields_=[(n,U32) for n in (
        'magic','version','checksum','cash','owned','unlocked_locations','paints',
        'best_score','best_distance','best_near','best_drift','best_speed_hold',
        'best_wheelie','best_coins','total_runs','total_distance','total_cash','seed')]+[
        ('selected_vehicle',U8),('selected_location',U8),('paint',U8*10),
        ('engine',U8*10),('grip',U8*10),('armor',U8*10),
        ('mastery_xp',U32*10),('career_xp',U32),('power_tune',U8*10),
        ('handling_tune',U8*10),('chassis_tune',U8*10),('perks',U8*3)]+[
        (n,U8) for n in ('music_on','sfx_on','music_volume','sfx_volume',
                        'skin','shake','units','motion')]+[('reserved',U8*3)]
class V3Save(C.LittleEndianStructure):
    _fields_=LegacySave._fields_[:18]+[
        ('selected_vehicle',U8),('selected_location',U8),('paint',U8*31),
        ('engine',U8*31),('grip',U8*31),('armor',U8*31),
        ('mastery_xp',U32*31),('career_xp',U32),('power_tune',U8*31),
        ('handling_tune',U8*31),('chassis_tune',U8*31),('perks',U8*3)]+[
        (n,U8) for n in ('music_on','sfx_on','music_volume','sfx_volume',
                        'skin','shake','units','motion')]+[
        ('recovery',U8*31),('livery',U8*31),('wheels',U8*31),
        ('livery_owned',U8*31),('wheels_owned',U8*31),('dispatch_choice',U8),
        ('contracts_completed',U32),('reserved',U8*4)]
class DiskSave3(C.LittleEndianStructure):
    _fields_=V3Save._fields_[:26]+[
        ('tunes',U8*31),('recovery',U8*31),('style',U8*31),
        ('livery_owned',U8*31),('wheels_owned',U8*31),('perks',U8*3)]+[
        (n,U8) for n in ('music_on','sfx_on','music_volume','sfx_volume',
                        'skin','shake','units','motion')]+[
        ('dispatch_choice',U8),('reserved',U8),('contracts_completed',U32)]
class Save(C.LittleEndianStructure):
    _fields_=V3Save._fields_+[('paint_seen',U8*31),('smoke',U8*31),('smoke_seen',U8*31),('presets',U32*3),('preset_mask',U8)]
class DispatchContract(C.LittleEndianStructure):
    _fields_=[('goal',U32),('reward',U32),('kind',U8),('reserved',U8*3)]
class VehicleDef(C.Structure):
    _fields_=[(n,C.c_char_p) for n in ('name','description','requirement')]+[
        (n,U16) for n in ('price','top_speed','acceleration','grip','armor')]+[
        (n,U8) for n in ('bike','unlock','class_id','specialty')]
class LocationDef(C.Structure):
    _fields_=[(n,C.c_char_p) for n in ('name','description','requirement')]+[
        ('price',U16),('unlock',U8)]
class RunStats(C.LittleEndianStructure):
    _fields_=[('count',U32*7),('points',U32*7)]+[(n,U32) for n in (
        'total_points','total_cash','distance_m','elapsed_ticks','max_speed',
        'max_combo_q8','best_speed_hold','drift_points','wheelie_points',
        'mastery_xp','career_xp','cash_bonus')]+[('banked',U8)]
class Traffic(C.LittleEndianStructure):
    _fields_=[('x_q8',I32),('z_q8',I32),('speed_q8',I32),('cruise_q8',I32),('serial',U16),
              ('kind',U8),('paint',U8),('active',U8),('near_awarded',U8),
              ('lane',U8),('target_lane',U8)]
class Pickup(C.LittleEndianStructure):
    _fields_=[('x_q8',I32),('z_q8',I32),('serial',U16),('active',U8),('reserved',U8)]
class Game(C.LittleEndianStructure):
    _fields_=[('save',Save),('run',RunStats),('traffic',Traffic*20),('pickups',Pickup*24)]+[
        (n,U32) for n in ('frame','run_seed','run_ticks')]+[
        (n,I32) for n in ('position_q8','furthest_q8','odometer_q8','x_q8',
                         'vx_q8','speed_q8','accel_q8','next_spawn_q8')]+[
        (n,U32) for n in ('speed_hold','drift_hold','wheelie_hold')]+[
        (n,U16) for n in ('combo_q8','damage_cooldown','crash_timer','horn_timer','shake','spawn_serial')]+[
        ('steering',I16),('yaw',I16)]+[(n,U8) for n in (
        'mode','cursor','selection','health','vehicle','start_biome','biome','turbo',
        'slow','drifting','wheelie','save_requested','save_ok','has_save','muted','paused_from')]+[
        ('notice_timer',U16),('notice',C.c_char*88),('paint_choice',U8),
        ('garage_tab',U8),('menu_return',U8),('repair_used',U8),('tune_choice',U8),
        ('turbo_charge_q8',U16),('turbo_latched',U8),('turbo_recharging',U8),
        ('turbo_source',U8),('controls_page',U8),('turbo_remainder',U16),
        ('police_ticks',U16),('police_pressure',U8),('end_reason',U8),
        ('garage_filter',U8),('livery_choice',U8),('wheel_choice',U8),('contract_complete',U8),
        ('active_contract',DispatchContract),('contract_progress',U32),('contract_cash',U32),('smoke_choice',U8),('build_choice',U8),('preset_choice',U8)]

def verify_layout(root:Path):
    """Compile every ctypes offset against C before observing cartridge RAM."""
    declarations=['#include "game.h"','#include <stddef.h>']
    for name,kind in (('Save',Save),('RunStats',RunStats),('Traffic',Traffic),
                      ('Pickup',Pickup),('Game',Game),('DispatchContract',DispatchContract),
                      ('VehicleDef',VehicleDef),('LocationDef',LocationDef)):
        declarations.append(f'_Static_assert(sizeof({name})=={C.sizeof(kind)},"{name} size");')
        for field,_ in kind._fields_:
            offset=getattr(kind,field).offset
            declarations.append(f'_Static_assert(offsetof({name},{field})=={offset},"{name}.{field}");')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-pedantic',
                    '-I',str(root/'src'),'-x','c','-fsyntax-only','-'],
                   input='\n'.join(declarations)+'\n',text=True,check=True)
    assert C.sizeof(LegacySave)==100 and C.sizeof(V2Save)==204 and C.sizeof(V3Save)==596
    assert C.sizeof(DiskSave3)==SAVE_DISK_SIZE
    assert C.sizeof(Save)==708 and C.sizeof(Game)==1808

MODES=('home','run','receipt','garage','departures','challenges','controls','pause','career','settings','contracts')
END_REASONS=('none','stopped by patrol','crash impound','banked voluntarily','route limit')
BONUSES=('distance','survival','near miss','speed hold','drift','wheelie','coins')
def symbol(elf:Path,name:str):
    text=subprocess.run(['readelf','-sW',str(elf)],check=True,capture_output=True,text=True).stdout
    for line in text.splitlines():
        parts=line.split()
        if len(parts)>=8 and parts[-1]==name:
            return int(parts[1],16),int(parts[2])
    raise RuntimeError(f'ELF symbol {name!r} not found in {elf}')
def find_symbol(elf:Path,name:str): return symbol(elf,name)[0]
def symbol_size(elf:Path,name:str): return symbol(elf,name)[1]
def read_game(emulator,address):
    return Game.from_buffer_copy(emulator.read_ram(address,C.sizeof(Game)))

def catalogue(library):
    """Read the actual host-compiled catalogue instead of duplicating its IDs."""
    vehicles=(VehicleDef*VEHICLE_COUNT).in_dll(library,'vehicles')
    locations=(LocationDef*LOCATION_COUNT).in_dll(library,'locations')
    def vehicle_info(i,v):
        return {'id':i,'name':v.name.decode(),'description':v.description.decode(),
                'price':int(v.price),'class_id':int(v.class_id),'specialty':int(v.specialty),
                'bike':bool(v.bike),'novelty':i>=25}
    return [vehicle_info(i,v) for i,v in enumerate(vehicles)],[
        {'id':i,'name':v.name.decode(),'description':v.description.decode(),
         'price':int(v.price),'unlock':int(v.unlock)} for i,v in enumerate(locations)]
