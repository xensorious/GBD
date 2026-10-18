# Asterwild: Lantern Isles

A new, original Nintendo DS creature-catching RPG opening. Arrive in Sunwake
Harbor, meet naturalist Mira, choose a companion, explore the island, earn wild
creatures' trust, help the gardens, and challenge Vale's lantern trial.

This is a native `.nds` game with new artwork, a new world, new characters and
new gameplay code. It does not require an existing commercial ROM or a patch.

This revision rebuilds exploration around four-direction tile movement and a
directly following overhead camera. The village, gardens and forest share an
authored layout with tall buildings, layered trees and a raised terrace reached
by sideways stairs. The near railing passes in front of the character. Selected
foliage sways, people and creatures breathe, and occasional leaves, fireflies
and coastal birds animate the surroundings. The six creature designs and the
battle, recruitment and progression systems remain from the original DS build.

## Playing on a phone

Import `Asterwild.nds` into Delta's Nintendo DS library. Use a DS controller skin
that exposes **Select, L and R**. Both screens are used; portrait orientation is
a comfortable starting point. All game actions use buttons. Touch is ignored.

| Button | Action |
| --- | --- |
| D-pad | Walk in four directions; navigate the active menu |
| A | Talk, meet a creature, choose, advance dialogue |
| B | Hold to run; cancel/back; retreat from a wild battle |
| Select | Switch focus between exploration and the lower-screen map/notebook |
| L / R | Cycle Map, Party, Journal, Guide and System while the lower screen has focus |
| Start | Open the System tab |
| X | Open Party; use a Dew Tonic on the selected companion in Party |
| Y | Open the Journal from exploration |

The lower screen displays the island map during exploration. Select gives the
notebook focus and pauses walking; Select returns focus to exploration. Battle,
conversation and companion-choice screens use the D-pad and A/B directly.

Mira waits at the Research Lodge northwest of the pier. Your map and journal
record the next step. Lantern camp and Mira provide free recovery. Wild battles
show the opponent's next intent. Strike builds energy, skills spend it, and
Guard restores energy while reducing the next hit. Befriend at half health, or
use Befriend twice to establish trust. There are no random recruitment rolls.

## Saving

Milestones save automatically. Press Select, use L/R to reach System, then choose
Save your journey to save manually. Continue on the title screen restores the cartridge battery save.
New expedition asks for confirmation when a save exists. Keep Delta's exported
DS save file with your ROM when backing up or moving progress. Emulator save
states are separate and can depend on the exact ROM version.

Asterwild has its own save format; Bramblebound GBA saves belong to that game.
Battery saves from the earlier Asterwild DS release keep their companions,
quest progress and trial results. The first load relocates the player to the
new harbor because the island layout has changed. Use a battery save when
moving between these releases; emulator save states contain the old program.

## Scope

This opening contains a complete first island and first trial, with six original
creature designs, three starter choices, visible encounters, a party, shared
experience, two growth paths, an optional blossom quest, a field guide, a
journal, audio and saves. The island remains explorable after the trial. Further
islands and an extended campaign are outside this first release.

## Build and test

```sh
bash tools/setup_nds.sh
python3 -m pip install -r ../requirements-dev.txt
make -j2
```

The setup uses a local ARM GCC/newlib toolchain and pinned open-source libnds,
CRT and ndstool revisions. It installs under `/workspace/nds-tools` without root.
`tools/world_layout.py` defines terrain, scenery footprints and landmarks on
the same 16-pixel lattice and regenerates `src/world.c`. `tools/topdown_art.py`
composes the terrain, illustrated props, depth silhouettes and wind poses.
Artwork in `art/` is converted to RGB555 DS assets by `tools/build_assets.py`;
no image-generation service is needed to rebuild the checked-in game.

See `tests/` for the native gameplay checks and actual DeSmuME ROM harness.
Generated assets, emulator screenshots, test saves and build products live in
ignored `build/`. DejaVu fonts provide the baked proportional text and title
lettering. Their permissive license is available from the DejaVu Fonts project.
