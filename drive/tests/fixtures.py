"""Explicit native/legacy battery-profile fixtures; never emulator RAM writes."""
from pathlib import Path
from game_snapshot import LegacySave


def legacy_profile():
    s=LegacySave();s.magic=0x434c4452;s.version=1;s.cash=12345
    s.owned=63;s.unlocked_locations=31;s.paints=63
    for name,value in {'best_score':34567,'best_distance':7000,'best_near':12,
        'best_drift':1600,'best_speed_hold':300,'best_wheelie':1200,
        'best_coins':37,'total_runs':17,'total_distance':45678,
        'total_cash':22345,'seed':0x12345abc}.items():setattr(s,name,value)
    s.selected_vehicle=3;s.selected_location=1
    for i in range(6):
        s.paint[i]=i;s.engine[i]=i%4;s.grip[i]=(i+1)%4;s.armor[i]=(i+2)%4
    value=2166136261
    for b in bytes(s):value=((value^b)*16777619)&0xffffffff
    s.checksum=value
    return s


def write_battery(template:Path,destination:Path,payload:bytes):
    data=template.read_bytes();raw_size=data.index(b'|<--Snip')
    if len(payload)>raw_size:raise ValueError('Save fixture exceeds EEPROM capacity')
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_bytes(payload+b'\xff'*(raw_size-len(payload))+data[raw_size:])
