"""Read-only ARM EABI view of game.h for emulator integration assertions.

The pointer in Game is explicitly 32-bit because the test host may be 64-bit.
These structures do not provide any write-back mechanism to emulator memory.
"""
import ctypes as C
import subprocess
from pathlib import Path


class Companion(C.LittleEndianStructure):
    _fields_ = [("species", C.c_uint8), ("level", C.c_uint8),
                ("xp", C.c_uint8), ("path", C.c_uint8),
                ("hp", C.c_uint16), ("energy", C.c_uint8),
                ("reserved", C.c_uint8)]


class Save(C.LittleEndianStructure):
    _fields_ = [("magic", C.c_uint32), ("version", C.c_uint32),
                ("checksum", C.c_uint32), ("flags", C.c_uint32),
                ("seed", C.c_uint32), ("player_x", C.c_int16),
                ("player_y", C.c_int16), ("facing", C.c_uint8),
                ("party_count", C.c_uint8), ("lead", C.c_uint8),
                ("starter", C.c_uint8), ("tonics", C.c_uint8),
                ("flowers", C.c_uint8), ("seen", C.c_uint8),
                ("recruited", C.c_uint8), ("wild_taken", C.c_uint8),
                ("reserved", C.c_uint8 * 3), ("battles_won", C.c_uint16),
                ("steps", C.c_uint16), ("party", Companion * 6)]


class Dialogue(C.LittleEndianStructure):
    _fields_ = [("speaker", C.c_char * 24),
                ("lines", (C.c_char * 112) * 8),
                ("count", C.c_uint8), ("index", C.c_uint8),
                ("after", C.c_uint8)]


class Battle(C.LittleEndianStructure):
    _fields_ = [("species", C.c_uint8), ("level", C.c_uint8),
                ("is_trial", C.c_uint8), ("encounter", C.c_uint8),
                ("hp", C.c_uint16), ("max_hp", C.c_uint16),
                ("turn", C.c_uint8), ("intent", C.c_uint8),
                ("phase", C.c_uint8), ("result", C.c_uint8),
                ("calm", C.c_uint8), ("guarded", C.c_uint8),
                ("active", C.c_uint8), ("log_count", C.c_uint8),
                ("log_index", C.c_uint8), ("flash_enemy", C.c_uint8),
                ("flash_player", C.c_uint8), ("effect", C.c_uint8),
                ("effect_timer", C.c_uint8),
                ("logs", (C.c_char * 112) * 8)]


class Game(C.LittleEndianStructure):
    _fields_ = [("save", Save), ("world", C.c_uint32),
                # The ARM toolchain uses the AAPCS small-enum convention.
                ("mode", C.c_uint8), ("return_mode", C.c_uint8),
                ("dialog", Dialogue), ("battle", Battle),
                ("frame", C.c_uint32), ("wild_cooldown", C.c_uint16 * 6),
                ("cursor", C.c_uint8), ("facing", C.c_uint8),
                ("walking", C.c_uint8), ("anim_tick", C.c_uint8),
                ("menu_from_battle", C.c_uint8), ("growth_slot", C.c_uint8),
                ("pending_growth", C.c_uint8), ("save_requested", C.c_uint8),
                ("save_feedback", C.c_uint8), ("has_save", C.c_uint8),
                ("title_cursor", C.c_uint8), ("ui_focus", C.c_uint8),
                ("ui_tab", C.c_uint8), ("new_confirm", C.c_uint8),
                ("toast_timer", C.c_uint16),
                ("interaction_cooldown", C.c_uint16),
                ("toast", C.c_char * 80)]


class Point(C.LittleEndianStructure):
    _fields_ = [("x", C.c_int16), ("y", C.c_int16)]


class Solid(C.LittleEndianStructure):
    _fields_ = [("x", C.c_int16), ("y", C.c_int16),
                ("w", C.c_int16), ("h", C.c_int16)]


class World(C.LittleEndianStructure):
    _fields_ = [("solids", C.c_uint32), ("solid_count", C.c_uint16),
                ("spawn", Point), ("camp", Point),
                ("npcs", Point * 4), ("wilds", Point * 6),
                ("flowers", Point * 3)]


MODES = ("title", "field", "dialog", "starter", "battle", "menu",
         "party", "journal", "guide", "growth", "ending")


def find_symbol(elf: Path, name: str) -> int:
    result = subprocess.run(["readelf", "-sW", str(elf)], check=True,
                            capture_output=True, text=True)
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 8 and fields[-1] == name:
            return int(fields[1], 16)
    raise RuntimeError(f"ELF symbol {name!r} not found in {elf}")


def read_game(emulator, address):
    return Game.from_buffer_copy(emulator.read_ram(address, C.sizeof(Game)))


def read_world(emulator, game):
    world = World.from_buffer_copy(emulator.read_ram(game.world, C.sizeof(World)))
    solids_type = Solid * world.solid_count
    solids = solids_type.from_buffer_copy(emulator.read_ram(world.solids, C.sizeof(solids_type)))
    return world, solids
