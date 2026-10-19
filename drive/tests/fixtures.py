"""Explicit native/legacy battery-profile fixtures; never emulator RAM writes."""
from pathlib import Path
import ctypes as C
import struct
from game_snapshot import LegacySave,V2Save


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
    old=legacy_profile();s=V2Save()
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


def encode_profile(library,game):
    """Use the cartridge codec; the 708-byte RAM view is never a disk fixture."""
    from game_snapshot import Game,Save,DiskSave3
    library.game_make_save.argtypes=[C.POINTER(Game),C.POINTER(Save)]
    library.game_save_valid.argtypes=[C.POINTER(Save)]
    canonical=Save();library.game_make_save(C.byref(game),C.byref(canonical))
    assert library.game_save_valid(C.byref(canonical)),'Invalid native review profile'
    library.game_encode_save.argtypes=[C.POINTER(Game),C.c_void_p,C.c_uint]
    library.game_encode_save.restype=C.c_int
    data=(C.c_uint8*500)()
    assert library.game_encode_save(C.byref(game),data,len(data))
    payload=bytes(data)
    assert len(payload)==500 and struct.unpack_from('<I',payload,4)[0]==4
    loaded=Game();library.game_init(C.byref(loaded),123)
    library.game_load_bytes.argtypes=[C.POINTER(Game),C.c_void_p,C.c_uint]
    assert library.game_load_bytes(C.byref(loaded),data,500)
    roundtrip=Save();library.game_make_save(C.byref(loaded),C.byref(roundtrip))
    assert bytes(roundtrip)==bytes(canonical)

    return payload


def battery_size(path:Path):
    """Physical chip geometry comes from the genuine emulator footer."""
    data=path.read_bytes();size=data.index(b'|<--Snip')
    assert b'|-DESMUME SAVE-|' in data[size:]
    return size


def assert_compact_battery(path:Path):
    """Independently validate all 500 serialized bytes on the original 512B chip."""
    assert battery_size(path)==512,'The cartridge changed the physical battery capacity'
    payload=bytearray(path.read_bytes()[:500])
    assert struct.unpack_from('<II',payload)==(0x434c4452,4)
    stored=struct.unpack_from('<I',payload,8)[0];payload[8:12]=b'\0'*4
    value=2166136261
    for byte in payload:value=((value^byte)*16777619)&0xffffffff
    assert value==stored,'The serialized v4 checksum does not cover the full 500-byte profile'
    assert payload[204:]!=b'\xff'*296,'No profile tail was written beyond the old payload'
    return {'serialized_save_bytes':500,'physical_save_bytes':512,'disk_checksum_valid':True}


def assert_imported_fields(current,old):
    """Check semantic preservation across changed RAM/disk array strides."""
    for name,_ in LegacySave._fields_[:18]:
        if name not in ('version','checksum'):
            assert getattr(current,name)==getattr(old,name),name
    for name in ('selected_vehicle','selected_location'):
        assert getattr(current,name)==getattr(old,name),name
    count=len(old.paint)
    for name in ('paint','engine','grip','armor'):
        values=list(getattr(current,name))
        assert values[:count]==list(getattr(old,name)),name
        assert not any(values[count:]),name
    if isinstance(old,V2Save):
        for name in ('mastery_xp','power_tune','handling_tune','chassis_tune'):
            values=list(getattr(current,name))
            assert values[:count]==list(getattr(old,name)),name
            assert not any(values[count:]),name
        for name in ('career_xp','music_on','sfx_on','music_volume','sfx_volume',
                     'skin','shake','units','motion'):
            assert getattr(current,name)==getattr(old,name),name
        assert list(current.perks)==list(old.perks)
    else:
        assert current.music_on and current.sfx_on
    assert current.version==4
    for name in ('recovery','livery','wheels'):
        assert not any(getattr(current,name)),name
    for name in ('livery_owned','wheels_owned'):
        assert list(getattr(current,name))==[1]*31,name
    assert current.dispatch_choice==3 and not current.contracts_completed
    assert not any(current.reserved)
