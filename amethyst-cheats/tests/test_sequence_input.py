"""Behavior checks for the HOST-SIDE reference model, not Delta validation."""

import unittest

from prototype.sequence_input import Button, Cheat, Command, DEFAULT_SEQUENCES, SequenceInput


class SequenceInputTests(unittest.TestCase):
    def setUp(self):
        self.decoder = SequenceInput()
        self.frame = 0

    def sample(self, mask, *, decoder=None):
        result = (decoder or self.decoder).sample(mask, self.frame)
        self.frame += 1
        return result

    def enter(self, sequence, *, hold_frames=1, decoder=None):
        commands = []
        for button in sequence:
            for _ in range(hold_frames):
                result = self.sample(button, decoder=decoder)
                if result is not None:
                    commands.append(result)
            self.sample(0, decoder=decoder)
        return commands

    def test_all_six_candidates_emit_distinct_expected_commands(self):
        expected = {
            Cheat.MASTER_BALL: Command(Cheat.MASTER_BALL, quantity=1),
            Cheat.MAX_REVIVE: Command(Cheat.MAX_REVIVE, quantity=1),
            Cheat.RARE_CANDY: Command(Cheat.RARE_CANDY, quantity=1),
            Cheat.WALK_ANY_TILE: Command(Cheat.WALK_ANY_TILE, enabled=True),
            Cheat.SHINY_ENCOUNTERS: Command(Cheat.SHINY_ENCOUNTERS, enabled=True),
            Cheat.INVULNERABILITY: Command(Cheat.INVULNERABILITY, enabled=True),
        }
        for cheat, sequence in DEFAULT_SEQUENCES.items():
            with self.subTest(cheat=cheat):
                self.assertEqual(self.enter(sequence), [expected[cheat]])

    def test_holding_buttons_and_final_key_emits_only_once(self):
        sequence = DEFAULT_SEQUENCES[Cheat.MASTER_BALL]
        self.assertEqual(self.enter(sequence[:-1], hold_frames=7), [])
        self.assertEqual(self.sample(sequence[-1]), Command(Cheat.MASTER_BALL, quantity=1))
        for _ in range(180):
            self.assertIsNone(self.sample(sequence[-1]))

    def test_repeated_button_needs_release(self):
        decoder = SequenceInput({Cheat.MASTER_BALL: (Button.A, Button.A)})
        self.assertIsNone(self.sample(Button.A, decoder=decoder))
        for _ in range(5):
            self.assertIsNone(self.sample(Button.A, decoder=decoder))
        self.sample(0, decoder=decoder)
        self.assertEqual(self.sample(Button.A, decoder=decoder), Command(Cheat.MASTER_BALL, quantity=1))

    def test_each_item_sequence_can_add_one_again(self):
        for cheat in (Cheat.MASTER_BALL, Cheat.MAX_REVIVE, Cheat.RARE_CANDY):
            with self.subTest(cheat=cheat):
                results = self.enter(DEFAULT_SEQUENCES[cheat]) + self.enter(DEFAULT_SEQUENCES[cheat])
                self.assertEqual(results, [Command(cheat, quantity=1)] * 2)

    def test_shiny_toggle_on_off_on(self):
        sequence = DEFAULT_SEQUENCES[Cheat.SHINY_ENCOUNTERS]
        results = self.enter(sequence) + self.enter(sequence) + self.enter(sequence)
        self.assertEqual(results, [Command(Cheat.SHINY_ENCOUNTERS, enabled=value) for value in (True, False, True)])

    def test_proposed_toggles_are_independent(self):
        shiny = DEFAULT_SEQUENCES[Cheat.SHINY_ENCOUNTERS]
        walk = DEFAULT_SEQUENCES[Cheat.WALK_ANY_TILE]
        invulnerable = DEFAULT_SEQUENCES[Cheat.INVULNERABILITY]
        self.assertEqual(self.enter(shiny), [Command(Cheat.SHINY_ENCOUNTERS, enabled=True)])
        self.assertEqual(self.enter(walk), [Command(Cheat.WALK_ANY_TILE, enabled=True)])
        self.assertEqual(self.enter(invulnerable), [Command(Cheat.INVULNERABILITY, enabled=True)])
        self.assertEqual(self.enter(walk), [Command(Cheat.WALK_ANY_TILE, enabled=False)])
        self.assertEqual(self.enter(shiny), [Command(Cheat.SHINY_ENCOUNTERS, enabled=False)])

    def test_wrong_button_cancels_partial_sequence(self):
        sequence = DEFAULT_SEQUENCES[Cheat.RARE_CANDY]
        self.assertEqual(self.enter(sequence[:5] + (Button.SELECT,) + sequence[5:]), [])
        self.assertEqual(self.enter(sequence), [Command(Cheat.RARE_CANDY, quantity=1)])

    def test_mismatch_retains_overlapping_prefix(self):
        sequence = DEFAULT_SEQUENCES[Cheat.RARE_CANDY]
        # The later L/R pair restarts the default L/R/L/R prefix.
        self.assertEqual(self.enter((Button.L, Button.R) + sequence), [Command(Cheat.RARE_CANDY, quantity=1)])

    def test_chord_cancels_and_requires_complete_release(self):
        decoder = SequenceInput({Cheat.RARE_CANDY: (Button.A, Button.B)})
        self.sample(Button.A, decoder=decoder)
        self.assertIsNone(self.sample(Button.A | Button.B, decoder=decoder))
        self.assertIsNone(self.sample(Button.A, decoder=decoder))
        self.assertIsNone(self.sample(Button.B, decoder=decoder))
        self.sample(0, decoder=decoder)
        self.assertEqual(self.enter((Button.A, Button.B), decoder=decoder), [Command(Cheat.RARE_CANDY, quantity=1)])

    def test_timeout_cancels_progress_including_held_key(self):
        decoder = SequenceInput({Cheat.RARE_CANDY: (Button.A, Button.B)}, timeout_frames=3)
        self.assertIsNone(decoder.sample(Button.A, 0))
        self.assertIsNone(decoder.sample(Button.A, 4))
        self.assertIsNone(decoder.sample(Button.B, 5))
        self.assertIsNone(decoder.sample(0, 6))
        self.assertIsNone(decoder.sample(Button.A, 7))
        self.assertEqual(decoder.sample(Button.B, 8), Command(Cheat.RARE_CANDY, quantity=1))

    def test_timeout_boundary_is_inclusive(self):
        decoder = SequenceInput({Cheat.RARE_CANDY: (Button.A, Button.B)}, timeout_frames=3)
        self.assertIsNone(decoder.sample(Button.A, 0))
        self.assertEqual(decoder.sample(Button.B, 3), Command(Cheat.RARE_CANDY, quantity=1))

    def test_different_single_keys_may_change_on_adjacent_samples(self):
        decoder = SequenceInput({Cheat.RARE_CANDY: (Button.A, Button.B)})
        self.assertIsNone(decoder.sample(Button.A, 0))
        self.assertEqual(decoder.sample(Button.B, 1), Command(Cheat.RARE_CANDY, quantity=1))

    def test_invalid_masks_and_frames_raise_without_consuming_sample(self):
        decoder = SequenceInput({Cheat.RARE_CANDY: (Button.A, Button.B)})
        self.assertIsNone(decoder.sample(Button.A, 10))
        for mask, frame in ((-1, 11), (0x400, 11), (0, 10), (0, 9), (0, -1)):
            with self.subTest(mask=mask, frame=frame), self.assertRaises(ValueError):
                decoder.sample(mask, frame)
        self.assertEqual(decoder.sample(Button.B, 11), Command(Cheat.RARE_CANDY, quantity=1))

    def test_configuration_rejects_empty_unequal_duplicate_or_chord_sequences(self):
        invalid = (
            {},
            {Cheat.MASTER_BALL: ()},
            {Cheat.MASTER_BALL: (Button.A,), Cheat.RARE_CANDY: (Button.A, Button.B)},
            {Cheat.MASTER_BALL: (Button.A,), Cheat.RARE_CANDY: (Button.A,)},
            {Cheat.MASTER_BALL: (Button.A | Button.B,)},
            {Cheat.MASTER_BALL: (Button(0),)},
            {Cheat.MASTER_BALL: (Button(0x400),)},
        )
        for sequences in invalid:
            with self.subTest(sequences=sequences), self.assertRaises(ValueError):
                SequenceInput(sequences)
        for timeout in (0, -1, 2.5):
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                SequenceInput(timeout_frames=timeout)


if __name__ == "__main__":
    unittest.main()
