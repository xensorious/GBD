"""Patch format, source identity, layout, and activation gating checks.

These use synthetic bytes; no game ROM is included or required.
"""
import struct
import tempfile
import unittest
from pathlib import Path
import zlib

from tools.build_release import (
    activation_code, apply_bps, build, make_bps, number, sections, validate_payload,
)
from tools.cheat_encoding import decode_line


def payload():
    return [('.text', 0x09FF0000, bytes(256)),
            ('.readkeys', 0x09FF1000, bytes(8)),
            ('.heapentry', 0x09FF1010, bytes(8)),
            ('.collision', 0x09FF1020, bytes(32)),
            ('.guards', 0x09FF1060,
             struct.pack('<5I', *(0x09FF0001 + i * 8 for i in range(5))))]


def elf_image(section_type=1):
    """Minimal ELF32 fixture containing one allocated .text section."""
    image = bytearray(52)
    names = b'\0.text\0.shstrtab\0'
    names_offset = len(image)
    image += names
    data_offset = len(image)
    image += b'\x70\x47'
    table_offset = len(image)
    headers = [(0,) * 10,
               (1, section_type, 6, 0x09FF0000, data_offset, 2, 0, 0, 2, 0),
               (7, 3, 0, 0, names_offset, len(names), 0, 0, 1, 0)]
    for header in headers:
        image += struct.pack('<10I', *header)
    image[:7] = b'\x7fELF\x01\x01\x01'
    struct.pack_into('<H', image, 18, 40)  # EM_ARM
    struct.pack_into('<I', image, 32, table_offset)
    struct.pack_into('<HHH', image, 46, 40, len(headers), 2)
    return image


def checked_patch(source, target, actions):
    patch = (b'BPS1' + number(len(source)) + number(len(target)) + b'\x80' + actions
             + struct.pack('<II', zlib.crc32(source), zlib.crc32(target)))
    return patch + struct.pack('<I', zlib.crc32(patch))


class BpsTests(unittest.TestCase):
    def test_spec_number_boundaries(self):
        for value, expected in ((0, '80'), (1, '81'), (127, 'ff'),
                                (128, '0080'), (255, '7f80'),
                                (256, '0081'), (16511, '7fff'),
                                (16512, '000080')):
            self.assertEqual(number(value).hex(), expected)
        with self.assertRaises(ValueError):
            number(-1)

    def test_hand_constructed_bps_fixture(self):
        # SourceRead(1), TargetRead("x"), SourceRead(1), per BPS v1 spec.
        patch = bytes.fromhex('4250533183838080817880c241243519bb098592c28fe3')
        self.assertEqual(apply_bps(b'abc', patch), b'axc')

    def test_empty_unchanged_and_mixed_runs(self):
        cases = [(b'', b''), (b'unchanged', b'unchanged'),
                 (b'abcdefghi', b'XbcdYfghZ'),
                 (bytes(32768), b'\xff' * 32768),
                 (bytes(range(256)) * 128, bytes(reversed(range(256))) * 128)]
        for source, target in cases:
            with self.subTest(size=len(source)):
                patch = make_bps(source, target)
                self.assertEqual(apply_bps(source, patch), target)
                self.assertEqual(struct.unpack('<I', patch[-4:])[0], zlib.crc32(patch[:-4]))
        with self.assertRaises(ValueError):
            make_bps(b'a', b'ab')

    def test_wrong_source_or_corrupt_patch_rejected(self):
        patch = make_bps(b'abc', b'axc')
        with self.assertRaises(ValueError):
            apply_bps(b'xbc', patch)
        for index in (0, 5, len(patch) - 8, len(patch) - 1):
            corrupt = bytearray(patch)
            corrupt[index] ^= 1
            with self.subTest(index=index), self.assertRaises(ValueError):
                apply_bps(b'abc', corrupt)
        with self.assertRaises(ValueError):
            apply_bps(b'abc', patch[:-1])

    def test_valid_crc_does_not_hide_malformed_actions(self):
        # Oversized read, missing target byte, unsupported copy, and trailing data.
        for actions in (b'\x8c', b'\x89x', b'\x82', b'\x88\x80'):
            patch = checked_patch(b'abc', b'abc', actions)
            with self.subTest(actions=actions), self.assertRaises(ValueError):
                apply_bps(b'abc', patch)


class PayloadTests(unittest.TestCase):
    def test_fixed_layout_accepts_only_runtime_thumb_guards(self):
        self.assertEqual(validate_payload(payload()),
                         tuple(0x09FF0001 + i * 8 for i in range(5)))
        for pointer in (0x09FF0000, 0x09FF0101, 0x08000101):
            parts = payload()
            parts[-1] = ('.guards', 0x09FF1060, struct.pack('<5I', *([pointer] * 5)))
            with self.subTest(pointer=pointer), self.assertRaises(ValueError):
                validate_payload(parts)

    def test_missing_duplicate_shifted_or_oversized_sections_rejected(self):
        malformed = [payload()[:-1], payload() + [payload()[0]],
                     [('.text', 0x0200A100, bytes(256))] + payload()[1:],
                     [('.text', 0x09FF0000, bytes(4097))] + payload()[1:],
                     payload()[:-1] + [('.guards', 0x09FF1060, bytes(16))]]
        for parts in malformed:
            with self.subTest(names=[p[0] for p in parts]), self.assertRaises(ValueError):
                validate_payload(parts)

    def test_elf_allocated_data_and_bad_headers(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'runtime.elf'
            path.write_bytes(elf_image())
            self.assertEqual(sections(path), [('.text', 0x09FF0000, b'\x70\x47')])
            fixtures = [b'not an elf', elf_image()[:-1], elf_image(section_type=8)]
            wrong_machine = elf_image()
            struct.pack_into('<H', wrong_machine, 18, 3)  # EM_386
            fixtures.append(wrong_machine)
            for image in fixtures:
                path.write_bytes(image)
                with self.assertRaises(ValueError):
                    sections(path)

    def test_wrong_rom_refused_before_reading_payload(self):
        with tempfile.TemporaryDirectory() as tmp:
            rom = Path(tmp) / 'wrong.gba'
            rom.write_bytes(b'wrong version')
            with self.assertRaisesRegex(ValueError, 'Wrong Amethyst build'):
                build(rom, Path(tmp) / 'does-not-exist.elf')


class ActivationTests(unittest.TestCase):
    def test_only_write_is_gated_by_patched_hook_and_reserved_state(self):
        # Raw AR v3 operations: two 32-bit global-AND conditions, heartbeat,
        # and codes-on terminator. The actual VBA engine is exercised separately.
        expected = [(0x8C8005EC, 0x09FF1001),
                    (0x8C21BE00, 0x414D5954),
                    (0x0421BE5C, 0x43484541),
                    (0, 0x40000000)]
        code = activation_code()
        self.assertEqual([decode_line(line) for line in code.splitlines()], expected)
        distributed = Path(__file__).resolve().parents[1] / 'dist/delta-action-replay.txt'
        self.assertEqual(distributed.read_text(), code)


if __name__ == '__main__':
    unittest.main()
