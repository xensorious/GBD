"""Format/operation checks only; these tests do not establish a game cheat."""

import unittest

from tools.cheat_encoding import (
    ar3_if_equal, ar3_rom_patch16, ar3_write, ar3_write_block, decode_line,
    encode_line, format_codes, gsa1_rom_patch16, gsa1_write,
)


def parse_ar3(code):
    address, value = decode_line(code)
    return (((address >> 25) & 127) | ((address >> 17) & 0x80),
            (address & 0x00F00000) << 4 | address & 0x0003FFFF, value)


class CheatEncodingTests(unittest.TestCase):
    def test_published_shiny_patch_vectors(self):
        # Existing encrypted code vectors, not a compatibility claim for a ROM.
        vectors = (
            ("C34A5E26 58D1FACD", 0, 0x1A020361),
            ("F935E412 9CD138A2", 0xE009, 0),
            ("4270C392 40AA6F6F", 0, 0x1C020362),
            ("AE649533 0ABCCE2C", 0xE209, 0),
        )
        for encrypted, address, value in vectors:
            with self.subTest(encrypted=encrypted):
                self.assertEqual(decode_line(encrypted), (address, value))
                self.assertEqual(encode_line(address, value), encrypted)
        self.assertEqual(ar3_rom_patch16(1, 0x080406C2, 0xE009),
                         (vectors[0][0], vectors[1][0]))
        self.assertEqual(ar3_rom_patch16(2, 0x080406C4, 0xE209),
                         (vectors[2][0], vectors[3][0]))

    def test_ar3_ram_address_layout_and_widths(self):
        for address in (0x02000000, 0x0203FFFC, 0x03000000, 0x03007FFC):
            for width, operation in ((8, 0), (16, 1), (32, 2)):
                with self.subTest(address=address, width=width):
                    value = (1 << width) - 1
                    self.assertEqual(parse_ar3(ar3_write(address, value, width=width)),
                                     (operation, address, value))

    def test_exact_block_write_does_not_touch_tail_bytes(self):
        data = bytes(range(1, 12))
        lines = ar3_write_block(0x02000101, data)
        writes = [parse_ar3(line) for line in lines]
        memory = {}
        for operation, address, value in writes:
            width = {0: 1, 1: 2, 2: 4}[operation]
            for offset, byte in enumerate(value.to_bytes(width, "little")):
                self.assertNotIn(address + offset, memory)
                memory[address + offset] = byte
        self.assertEqual(memory, {0x02000101 + i: value for i, value in enumerate(data)})
        self.assertEqual(ar3_write_block(0x02000100, b""), [])

    def test_gate_operations_encode_one_or_two_line_skips(self):
        for width, base in ((8, 4), (16, 5), (32, 6)):
            for skip in (1, 2):
                expected_operation = base + (0x20 if skip == 2 else 0)
                code = ar3_if_equal(0x02000100, 1, width=width, skip=skip)
                self.assertEqual(parse_ar3(code), (expected_operation, 0x02000100, 1))

    def test_all_rom_slots_and_last_cartridge_halfword(self):
        for slot in range(4):
            header, value = ar3_rom_patch16(slot, 0x09FFFFFE, 0xBEEF)
            self.assertEqual(decode_line(header), (0, (0x0C + slot) << 25 | 0xFFFFFF))
            self.assertEqual(decode_line(value), (0xBEEF, 0))

    def test_v1_raw_operation_encoding(self):
        self.assertEqual(decode_line(gsa1_rom_patch16(0x080005EC, 0x1234), version=1),
                         (0x640002F6, 0x1234))
        for width, operation in ((8, 0), (16, 1), (32, 2)):
            code = gsa1_write(0x02000100, 1, width=width)
            self.assertEqual(decode_line(code, version=1),
                             (operation << 28 | 0x02000100, 1))

    def test_bad_values_addresses_and_slot_limits(self):
        invalid_calls = (
            lambda: ar3_write(0x02000001, 1),
            lambda: ar3_write(0x08000000, 1),
            lambda: ar3_write(0x03008000, 1),
            lambda: ar3_write(0x02000000, 256, width=8),
            lambda: ar3_write(0x02000000, -1),
            lambda: ar3_write(0x02000000, True),
            lambda: ar3_write_block(0x0203FFFF, b"ab"),
            lambda: ar3_rom_patch16(4, 0x08000000, 1),
            lambda: ar3_rom_patch16(-1, 0x08000000, 1),
            lambda: ar3_rom_patch16(0, 0x08000001, 1),
            lambda: ar3_rom_patch16(0, 0x0A000000, 1),
            lambda: ar3_if_equal(0x02000000, 1, skip=3),
            lambda: encode_line(0, 0, version=2),
            lambda: decode_line("0000 0000"),
            lambda: decode_line("00000000 Z0000000"),
        )
        for call in invalid_calls:
            with self.assertRaises(ValueError):
                call()

    def test_output_is_delta_formatted(self):
        lines = list(ar3_rom_patch16(0, 0x08000000, 0))
        text = format_codes(lines)
        self.assertEqual(text.splitlines(), lines)
        for line in text.splitlines():
            self.assertRegex(line, r"^[A-F0-9]{8} [A-F0-9]{8}$")


if __name__ == "__main__":
    unittest.main()
