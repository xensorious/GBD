"""Explicit native/legacy battery-profile fixtures; never emulator RAM writes."""
from pathlib import Path
from game_snapshot import LegacySave,Save


def checksum_profile(profile):
    profile.checksum=0
    value=2166136261
    for b in bytes(profile):value=((value^b)*16777619)&0xffffffff
    profile.checksum=value
    return profile


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
    return checksum_profile(s)


def coastal_profile():
    """Documented 204-byte v2 progress/settings fixture for release compatibility."""
    old=legacy_profile();s=Save()
    for name,_ in LegacySave._fields_:
        if name not in ('paint','engine','grip','armor','reserved'):
            setattr(s,name,getattr(old,name))
    s.version=2;s.owned=1023;s.unlocked_locations=255
    s.selected_vehicle=9;s.selected_location=7;s.career_xp=22000
    for i in range(10):
        s.paint[i]=i%6;s.engine[i]=i%7;s.grip[i]=(i+2)%7;s.armor[i]=(i+4)%7
        s.mastery_xp[i]=10000+i*23
        s.power_tune[i]=i%3;s.handling_tune[i]=(i+1)%3;s.chassis_tune[i]=(i+2)%3
    s.perks[:]=(3,2,1)
    s.music_on=0;s.sfx_on=1;s.music_volume=45;s.sfx_volume=65
    s.skin=2;s.shake=0;s.units=1;s.motion=2
    return checksum_profile(s)


def write_battery(template:Path,destination:Path,payload:bytes):
    data=template.read_bytes();raw_size=data.index(b'|<--Snip')
    if len(payload)>raw_size:raise ValueError('Save fixture exceeds EEPROM capacity')
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_bytes(payload+b'\xff'*(raw_size-len(payload))+data[raw_size:])
