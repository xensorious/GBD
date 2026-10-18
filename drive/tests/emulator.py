#!/usr/bin/env python3
"""Small headless libretro frontend for genuine Nintendo DS ROM validation.

The core executes the shipped ARM9/ARM7 binaries. Input is supplied through the
same joypad/pointer callbacks as a frontend; this harness never writes game RAM.
Pillow is needed only when saving screenshots. No BIOS, GUI, or display server
is needed for the DeSmuME homebrew direct-boot path.
"""

from __future__ import annotations

import argparse
import array
import ctypes as C
import json
import os
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CORE = Path("/workspace/nds-qa-tools/sysroot/usr/lib/x86_64-linux-gnu/libretro/desmume_libretro.so")
DEFAULT_XML = DEFAULT_CORE.parents[1] / "libtinyxml.so.2.6.2"


class GameInfo(C.Structure):
    _fields_ = [("path", C.c_char_p), ("data", C.c_void_p),
                ("size", C.c_size_t), ("meta", C.c_char_p)]


class SystemInfo(C.Structure):
    _fields_ = [("name", C.c_char_p), ("version", C.c_char_p),
                ("extensions", C.c_char_p), ("need_fullpath", C.c_bool),
                ("block_extract", C.c_bool)]


class Variable(C.Structure):
    _fields_ = [("key", C.c_char_p), ("value", C.c_char_p)]


class Geometry(C.Structure):
    _fields_ = [("base_width", C.c_uint), ("base_height", C.c_uint),
                ("max_width", C.c_uint), ("max_height", C.c_uint),
                ("aspect_ratio", C.c_float)]


class Timing(C.Structure):
    _fields_ = [("fps", C.c_double), ("sample_rate", C.c_double)]


class AVInfo(C.Structure):
    _fields_ = [("geometry", Geometry), ("timing", Timing)]


ENV = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
VIDEO = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
AUDIO = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
AUDIO_BATCH = C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t)
INPUT_POLL = C.CFUNCTYPE(None)
INPUT_STATE = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)

# Standard libretro button IDs, deliberately distinct from the DS register bits.
BUTTONS = {"b": 0, "y": 1, "select": 2, "start": 3,
           "up": 4, "down": 5, "left": 6, "right": 7,
           "a": 8, "x": 9, "l": 10, "r": 11}


class Emulator:
    def __init__(self, rom: Path | str, save_dir: Path | str | None = None,
                 core: Path | str | None = None):
        self.rom = Path(rom).resolve()
        self.save_dir = Path(save_dir or ROOT / "build" / "qa-save").resolve()
        self.save_dir.mkdir(parents=True, exist_ok=True)
        self.options = {
            b"desmume_internal_resolution": b"256x192",
            b"desmume_num_cores": b"1",
            # This old Debian core has an ARM7 FIFO/JIT regression. The
            # interpreter correctly executes libnds audio and IPC handlers.
            b"desmume_cpu_mode": b"interpreter",
            b"desmume_screens_layout": b"top/bottom",
            b"desmume_pointer_mouse": b"enable",
            b"desmume_pointer_device": b"none",
            b"desmume_pointer_type": b"touch",
            b"desmume_pointer_stylus_jitter": b"disable",
            b"desmume_frameskip": b"0",
            b"desmume_firmware_language": b"English",
            b"desmume_screens_gap": b"0",
        }
        self.pixel_format = 0
        self.width = 0
        self.height = 0
        self.pitch = 0
        self.framebuffer = b""
        self.frames = 0
        self.video_frames = 0
        self.audio_samples = 0
        self.audio_nonzero = 0
        self._audio_probe = None
        self.keys = set()
        self.touch = None
        self.messages = []
        self.shutdown = False
        self._save_bytes = os.fsencode(self.save_dir)
        self._system_bytes = os.fsencode(self.save_dir / "system")
        self._content_bytes = os.fsencode(self.rom.parent)
        Path(os.fsdecode(self._system_bytes)).mkdir(exist_ok=True)
        self._rom_bytes = os.fsencode(self.rom)
        self._rom_data = None
        self._snapshot_frame = -1
        self._snapshot_fields = {}
        core_path = Path(core or os.environ.get("NDS_LIBRETRO_CORE", DEFAULT_CORE))
        xml = Path(os.environ.get("NDS_LIBRETRO_XML", DEFAULT_XML))
        if xml.exists():
            C.CDLL(str(xml), mode=C.RTLD_GLOBAL)
        self.lib = C.CDLL(str(core_path))
        self._callbacks = (ENV(self._environment), VIDEO(self._video),
                           AUDIO(self._audio), AUDIO_BATCH(self._audio_batch),
                           INPUT_POLL(lambda: None), INPUT_STATE(self._input))
        names = ("environment", "video_refresh", "audio_sample",
                 "audio_sample_batch", "input_poll", "input_state")
        for name, callback in zip(names, self._callbacks):
            fn = getattr(self.lib, "retro_set_" + name)
            fn.argtypes = [type(callback)]
            fn(callback)
        self.lib.retro_get_system_info.argtypes = [C.POINTER(SystemInfo)]
        self.lib.retro_get_system_av_info.argtypes = [C.POINTER(AVInfo)]
        self.lib.retro_load_game.argtypes = [C.POINTER(GameInfo)]
        self.lib.retro_load_game.restype = C.c_bool
        self.lib.retro_get_memory_data.argtypes = [C.c_uint]
        self.lib.retro_get_memory_data.restype = C.c_void_p
        self.lib.retro_get_memory_size.argtypes = [C.c_uint]
        self.lib.retro_get_memory_size.restype = C.c_size_t
        self.lib.retro_serialize_size.restype = C.c_size_t
        self.lib.retro_serialize.argtypes = [C.c_void_p, C.c_size_t]
        self.lib.retro_serialize.restype = C.c_bool
        self.lib.retro_unserialize.argtypes = [C.c_void_p, C.c_size_t]
        self.lib.retro_unserialize.restype = C.c_bool
        self.lib.retro_init()
        info = SystemInfo()
        self.lib.retro_get_system_info(C.byref(info))
        self.core_name = info.name.decode()
        self.core_version = info.version.decode()
        data = None
        size = 0
        if not info.need_fullpath:
            content = self.rom.read_bytes()
            self._rom_data = C.create_string_buffer(content)
            data = C.cast(self._rom_data, C.c_void_p)
            size = len(content)
        game = GameInfo(self._rom_bytes, data, size, None)
        if not self.lib.retro_load_game(C.byref(game)):
            self.lib.retro_deinit()
            raise RuntimeError(f"{self.core_name} rejected {self.rom}")
        av = AVInfo()
        self.lib.retro_get_system_av_info(C.byref(av))
        self.fps = av.timing.fps
        self.closed = False

    def _environment(self, command, data):
        # GET_CAN_DUPE, GET_VARIABLE_UPDATE, GET_OVERSCAN
        if command in (3, 17, 2):
            C.cast(data, C.POINTER(C.c_bool))[0] = command == 3
            return True
        if command in (9, 30, 31):
            value = {9: self._system_bytes, 30: self._content_bytes,
                     31: self._save_bytes}[command]
            C.cast(data, C.POINTER(C.c_char_p))[0] = value
            return True
        if command == 10:  # SET_PIXEL_FORMAT
            self.pixel_format = C.cast(data, C.POINTER(C.c_int))[0]
            return self.pixel_format in (0, 1, 2)
        if command == 15:  # GET_VARIABLE
            variable = C.cast(data, C.POINTER(Variable)).contents
            value = self.options.get(variable.key)
            if value is None:
                return False
            variable.value = value
            return True
        if command == 16:  # SET_VARIABLES: retain defaults for unlisted options.
            variables = C.cast(data, C.POINTER(Variable))
            index = 0
            while variables[index].key:
                key, value = variables[index].key, variables[index].value
                if key not in self.options and value and b"; " in value:
                    self.options[key] = value.split(b"; ", 1)[1].split(b"|", 1)[0]
                index += 1
            return True
        if command == 6:  # SET_MESSAGE
            message = C.cast(data, C.POINTER(C.c_char_p))[0]
            if message:
                self.messages.append(message.decode(errors="replace"))
            return True
        if command == 7:
            self.shutdown = True
            return True
        if command == 37:  # GET_LANGUAGE: English
            C.cast(data, C.POINTER(C.c_uint))[0] = 0
            return True
        if command in (18, 11, 32, 34, 44):
            return True
        # GET_LOG_INTERFACE and hardware GL intentionally unsupported.
        return False

    def _video(self, data, width, height, pitch):
        if not data or data == C.c_void_p(-1).value:
            return
        self.framebuffer = C.string_at(data, pitch * height)
        self.width, self.height, self.pitch = width, height, pitch
        self.video_frames += 1

    def _audio(self, left, right):
        self.audio_samples += 1
        self.audio_nonzero += bool(left or right)
        if self._audio_probe is not None:
            self._audio_probe[0] += left*left+right*right
            self._audio_probe[1] += 2

    def _audio_batch(self, data, count):
        self.audio_samples += count
        if data and count:
            raw = C.string_at(data, count * 4)
            self.audio_nonzero += any(raw)
            if self._audio_probe is not None:
                values = array.array("h")
                values.frombytes(raw)
                observed = values[::8]
                self._audio_probe[0] += sum(v*v for v in observed)
                self._audio_probe[1] += len(observed)
        return count

    def begin_audio_probe(self):
        """Observe sample energy without altering the cartridge or audio."""
        self._audio_probe = [0, 0]

    def end_audio_probe(self):
        squared, count = self._audio_probe or (0, 0)
        self._audio_probe = None
        return {"samples_observed": count,
                "rms": (squared / count) ** .5 if count else 0.0}

    def _input(self, port, device, index, key):
        if port != 0:
            return 0
        if device == 1:  # JOYPAD
            if key == 256:  # optional full bitmask
                return sum(1 << BUTTONS[k] for k in self.keys)
            return int(any(BUTTONS[name] == key for name in self.keys))
        if device == 6 and self.touch is not None:  # POINTER
            x, y = self.touch
            if key == 2:
                return 1
            if key == 0:
                return int((x + .5) * 65536 / 256 - 32768)
            if key == 1:
                return int((y + 192 + .5) * 65536 / 384 - 32768)
        return 0

    def run(self, frames=1, keys=(), touch=None):
        self.keys = set(keys)
        unknown = self.keys - BUTTONS.keys()
        if unknown:
            raise ValueError(f"Unknown buttons: {unknown}")
        self.touch = touch
        for _ in range(frames):
            if self.shutdown:
                raise RuntimeError("ROM requested frontend shutdown")
            self.lib.retro_run()
            self.frames += 1

    def press(self, key, frames=2, release=3):
        self.run(frames, (key,))
        self.run(release)

    def tap(self, x, y, frames=3, release=4):
        """Touch coordinates are local to the 256x192 bottom screen."""
        if not (0 <= x < 256 and 0 <= y < 192):
            raise ValueError("Touch outside the bottom screen")
        self.run(frames, touch=(x, y))
        self.run(release)

    def image(self):
        from PIL import Image
        if not self.framebuffer:
            raise RuntimeError("No video frame received")
        if self.pixel_format == 1:
            return Image.frombytes("RGB", (self.width, self.height),
                                   self.framebuffer, "raw", "BGRX", self.pitch)
        values = array.array("H")
        values.frombytes(self.framebuffer)
        stride = self.pitch // 2
        rgb = bytearray(self.width * self.height * 3)
        for y in range(self.height):
            for x in range(self.width):
                pixel = values[y * stride + x]
                if self.pixel_format == 2:
                    r, g, b = (pixel >> 11) & 31, (pixel >> 5) & 63, pixel & 31
                    color = (r * 255 // 31, g * 255 // 63, b * 255 // 31)
                else:
                    r, g, b = (pixel >> 10) & 31, (pixel >> 5) & 31, pixel & 31
                    color = (r * 255 // 31, g * 255 // 31, b * 255 // 31)
                offset = (y * self.width + x) * 3
                rgb[offset:offset + 3] = bytes(color)
        return Image.frombytes("RGB", (self.width, self.height), bytes(rgb))

    def screenshot(self, path):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        self.image().save(path)
        return path

    def read_ram(self, address, count):
        """Read-only observation; DS main RAM starts at 0x02000000."""
        pointer = self.lib.retro_get_memory_data(2)
        size = self.lib.retro_get_memory_size(2)
        offset = address - 0x02000000
        if not pointer:
            # Debian's 2016 core predates the RAM API. Its uncompressed
            # serialization contains a tagged WRAM record; observing this is
            # equivalent and never restores or alters emulated state.
            memory = self.snapshot_fields().get((4, b"WRAM"), b"")
            if offset < 0 or offset + count > len(memory):
                raise ValueError(f"RAM range unavailable: {address:#x}, {count}")
            return memory[offset:offset + count]
        if offset < 0 or offset + count > size:
            raise ValueError(f"RAM range unavailable: {address:#x}, {count}")
        return C.string_at(pointer + offset, count)

    def snapshot_fields(self):
        if self._snapshot_frame == self.frames:
            return self._snapshot_fields
        state = self.save_state()
        if state[:16] != b"DeSmuME SState\x00\x00":
            raise RuntimeError(f"Unrecognized emulator state: {state[:16]!r}")
        length, compressed = struct.unpack_from("<II", state, 24)
        if compressed != 0xFFFFFFFF:
            raise RuntimeError("Expected an uncompressed DeSmuME state")
        fields = {}
        position = 32
        while position + 8 <= length:
            kind, size = struct.unpack_from("<II", state, position)
            position += 8
            end = position + size
            if end > length or kind == 0xFFFFFFFF:
                break
            if kind in (1, 2, 4):
                cursor = position
                while cursor + 12 <= end:
                    tag, item_size, count = struct.unpack_from("<4sII", state, cursor)
                    cursor += 12
                    byte_count = item_size * count
                    if cursor + byte_count > end:
                        raise RuntimeError("Malformed emulator state record")
                    fields[kind, tag] = state[cursor:cursor + byte_count]
                    cursor += byte_count
            position = end
        self._snapshot_fields = fields
        self._snapshot_frame = self.frames
        return fields

    def save_state(self):
        """Emulator checkpoint for test branches; separate from battery saves."""
        size = self.lib.retro_serialize_size()
        buffer = C.create_string_buffer(size)
        if not self.lib.retro_serialize(buffer, size):
            raise RuntimeError("Core could not serialize")
        return buffer.raw

    def load_state(self, state):
        buffer = C.create_string_buffer(state)
        if not self.lib.retro_unserialize(buffer, len(state)):
            raise RuntimeError("Core could not restore checkpoint")

    def close(self):
        if not self.closed:
            self.lib.retro_unload_game()
            self.lib.retro_deinit()
            self.closed = True

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("--frames", type=int, default=180)
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "boot.png")
    parser.add_argument("--save-dir", type=Path)
    args = parser.parse_args()
    with Emulator(args.rom, args.save_dir) as emu:
        emu.run(args.frames)
        emu.screenshot(args.output)
        print(json.dumps({"core": emu.core_name, "version": emu.core_version,
                          "frames": emu.frames, "video_frames": emu.video_frames,
                          "resolution": [emu.width, emu.height],
                          "screenshot": str(args.output),
                          "audio_samples": emu.audio_samples,
                          "audio_nonzero_batches": emu.audio_nonzero}, indent=2))


if __name__ == "__main__":
    main()
