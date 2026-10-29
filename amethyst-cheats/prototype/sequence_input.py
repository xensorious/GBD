"""Host-side reference prototype for deliberate GBA button-sequence commands.

This Python module cannot be pasted into Delta and does not modify a ROM,
inventory, encounters, collision, or battles. It specifies input behavior for a
future, separately verified implementation in the game or cheat engine.

Inputs are ACTIVE-HIGH GBA button masks: a set bit means pressed. Raw GBA
KEYINPUT is active-low; normalize it with ``(~keyinput) & 0x03FF`` first.
Call ``sample(mask, frame)`` once per observed frame, using strictly increasing,
nonnegative frame numbers. The timeout is an inclusive maximum gap between
accepted taps; a gap greater than ``timeout_frames`` clears partial progress.

The default sequences are candidates, not finalized user controls. Walking and
invulnerability toggles are proposed behavior; only the shiny toggle was an
explicit user requirement. All toggles start off in a new decoder instance.
Reset/save-state persistence is outside this reference model.
"""

from dataclasses import dataclass
from enum import Enum, IntFlag
from typing import Mapping, Sequence


class Button(IntFlag):
    A = 0x001
    B = 0x002
    SELECT = 0x004
    START = 0x008
    RIGHT = 0x010
    LEFT = 0x020
    UP = 0x040
    DOWN = 0x080
    R = 0x100
    L = 0x200


class Cheat(str, Enum):
    MASTER_BALL = "master_ball"
    MAX_REVIVE = "max_revive"
    RARE_CANDY = "rare_candy"
    WALK_ANY_TILE = "walk_any_tile"
    SHINY_ENCOUNTERS = "shiny_encounters"
    INVULNERABILITY = "invulnerability"


@dataclass(frozen=True)
class Command:
    """A requested effect, not evidence that anything changed in the game."""

    cheat: Cheat
    quantity: int | None = None
    enabled: bool | None = None


_PREFIX = (
    Button.L, Button.R, Button.L, Button.R,
    Button.LEFT, Button.RIGHT, Button.UP, Button.DOWN,
)

# All six candidates have ten taps. Repeated taps require a release between
# them; there are no Start/Select inputs that would open the game's menus.
DEFAULT_SEQUENCES = {
    Cheat.MASTER_BALL: _PREFIX + (Button.A, Button.A),
    Cheat.MAX_REVIVE: _PREFIX + (Button.B, Button.B),
    Cheat.RARE_CANDY: _PREFIX + (Button.A, Button.B),
    Cheat.WALK_ANY_TILE: _PREFIX + (Button.B, Button.A),
    Cheat.SHINY_ENCOUNTERS: _PREFIX + (Button.L, Button.L),
    Cheat.INVULNERABILITY: _PREFIX + (Button.R, Button.R),
}

_ITEMS = {Cheat.MASTER_BALL, Cheat.MAX_REVIVE, Cheat.RARE_CANDY}
_TOGGLES = set(Cheat) - _ITEMS
_ALL_BUTTONS = 0x03FF


class SequenceInput:
    """Emit one command per completed sequence, with held-key suppression.

    A changed single-button mask is a new tap, even if the previous button's
    release and the new button's press occur in the same sampled frame. The
    same button needs a zero sample before it counts again. A multi-button
    chord clears progress and blocks input until every button is released.
    Wrong taps retain the longest suffix that can start a valid sequence.
    Completed sequences are consumed and do not overlap subsequent commands.
    """

    def __init__(
        self,
        sequences: Mapping[Cheat, Sequence[Button]] | None = None,
        *,
        timeout_frames: int = 120,
    ) -> None:
        if not isinstance(timeout_frames, int) or timeout_frames < 1:
            raise ValueError("timeout_frames must be a positive integer")
        source = DEFAULT_SEQUENCES if sequences is None else sequences
        self._sequences = {
            Cheat(cheat): tuple(Button(button) for button in sequence)
            for cheat, sequence in source.items()
        }
        lengths = {len(sequence) for sequence in self._sequences.values()}
        if len(lengths) != 1 or not next(iter(lengths)):
            raise ValueError("sequences must be nonempty and have equal length")
        if len(set(self._sequences.values())) != len(self._sequences):
            raise ValueError("each cheat must have a distinct sequence")
        for sequence in self._sequences.values():
            for button in sequence:
                value = int(button)
                if value < 1 or value & ~_ALL_BUTTONS or value & (value - 1):
                    raise ValueError("sequences must contain single GBA buttons")
        self.timeout_frames = timeout_frames
        self._progress: tuple[Button, ...] = ()
        self._previous_mask = 0
        self._release_required = False
        self._last_frame = -1
        self._last_tap_frame: int | None = None
        self._toggle_states = dict.fromkeys(_TOGGLES, False)

    def sample(self, mask: int, frame: int) -> Command | None:
        """Process one sample and return a command only on sequence completion.

        Invalid masks or frame numbers raise ValueError without consuming the
        sample. Multi-button masks are valid samples but cancel the sequence.
        """
        if not isinstance(mask, int) or mask < 0 or mask & ~_ALL_BUTTONS:
            raise ValueError("mask must contain only the ten GBA button bits")
        if not isinstance(frame, int) or frame < 0 or frame <= self._last_frame:
            raise ValueError("frame numbers must be nonnegative and increasing")
        self._last_frame = frame

        if (
            self._last_tap_frame is not None
            and frame - self._last_tap_frame > self.timeout_frames
        ):
            self._clear_progress()

        previous = self._previous_mask
        self._previous_mask = int(mask)
        if mask == 0:
            self._release_required = False
            return None
        if mask & (mask - 1):
            self._clear_progress()
            self._release_required = True
            return None
        if self._release_required or mask == previous:
            return None

        self._last_tap_frame = frame
        self._progress += (Button(mask),)
        while self._progress and not any(
            sequence[:len(self._progress)] == self._progress
            for sequence in self._sequences.values()
        ):
            self._progress = self._progress[1:]

        for cheat, sequence in self._sequences.items():
            if self._progress == sequence:
                self._clear_progress()
                if cheat in _ITEMS:
                    return Command(cheat=cheat, quantity=1)
                self._toggle_states[cheat] = not self._toggle_states[cheat]
                return Command(cheat=cheat, enabled=self._toggle_states[cheat])
        return None

    def _clear_progress(self) -> None:
        self._progress = ()
        self._last_tap_frame = None
