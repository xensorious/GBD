import struct
import unittest

from tools.code_only_relocate import _clone, _instructions, _bl, _bl_target, clone_parts


class CallbackRelocationTests(unittest.TestCase):
    def fixture(self):
        # LDR literal; external BL; POP PC; internal Thumb pointer literal.
        return (struct.pack('<H', 0x4801) + _bl(0x08000002, 0x08000020)
                + struct.pack('<H', 0xBD00) + struct.pack('<I', 0x08000001)
                + bytes(52))

    def test_external_call_uses_preserving_veneer_and_literal_rebases(self):
        result = _clone(self.fixture(), 0x08000000, 0x0800000C,
                        0x0200E000, 0x0200F000, {})
        self.assertEqual(struct.unpack_from('<I', result, 8)[0], 0x0200E001)
        a, b = struct.unpack_from('<HH', result, 2)
        self.assertEqual(_bl_target(0x0200E002, a, b), 0x0200E00C)
        self.assertEqual(result[12:24], bytes.fromhex('08b4024b9c4608bc6047c046'))
        self.assertEqual(struct.unpack_from('<I', result, 24)[0], 0x08000021)

    def test_only_actual_pc_literals_can_be_replaced(self):
        result = _clone(self.fixture(), 0x08000000, 0x0800000C,
                        0x0200E000, 0x0200F000, {0x08000008: 0x02010038})
        self.assertEqual(struct.unpack_from('<I', result, 8)[0], 0x02010038)
        with self.assertRaisesRegex(ValueError, 'actual PC literal'):
            _clone(self.fixture(), 0x08000000, 0x0800000C,
                   0x0200E000, 0x0200F000, {0x08000000: 0})

    def test_literal_data_is_not_decoded_as_instructions(self):
        instructions, calls, literals = _instructions(self.fixture(), 0x08000000, 0x0800000C)
        self.assertEqual(set(instructions), {0x08000000, 0x08000002, 0x08000006})
        self.assertEqual(literals, {0x08000008})
        self.assertEqual(calls, {0x08000002: 0x08000020})

    def test_rejects_clone_overflow(self):
        with self.assertRaisesRegex(ValueError, 'exceeds reserved RAM'):
            _clone(self.fixture(), 0x08000000, 0x0800000C,
                   0x0200E000, 0x0200E010, {})

    def test_rejects_code_data_overlap(self):
        data = struct.pack('<HHHH', 0x4800, 0x46C0, 0xBD00, 0)
        with self.assertRaisesRegex(ValueError, 'overlap'):
            _instructions(data, 0x08000000, 0x08000008)

    def test_rejects_external_pc_relative_address(self):
        data = struct.pack('<HH', 0xA00F, 0xBD00)
        with self.assertRaisesRegex(ValueError, 'External ADR'):
            _instructions(data, 0x08000000, 0x08000004)

    def test_rejects_branch_outside_bounded_function(self):
        with self.assertRaisesRegex(ValueError, 'outside its bounds'):
            _instructions(struct.pack('<H', 0xE010), 0x08000000, 0x08000002)

    def test_rejects_different_rom_before_relocation(self):
        with self.assertRaisesRegex(ValueError, 'exact Amethyst'):
            clone_parts(b'unrelated ROM', 0x0200D001)


if __name__ == '__main__':
    unittest.main()
