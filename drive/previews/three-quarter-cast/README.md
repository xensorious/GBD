# Coastline: three-quarter conversation cast

Open `Coastline.nds` directly in Delta. This is the complete game, with the
artwork inside the ROM; no extraction or separate asset folder is required.

All 28 characters now have independently authored left- and right-facing
three-quarter full-body views. The 142 additions cover every available outfit
and expression: five poses for Nia, four for each other story character and one
for each of the fourteen unassigned concepts. Nia retains her white bra top,
denim cutoffs, brown belt, bare feet, long brown hair and physical-side tattoos.
The preceding 76 portraits remain available, bringing the archive to 218.

Standing conversations in the eight floor-compatible settings use inward-facing
partners. Their places stay stable when the speaker changes; a small depth
offset, contact shadows and subtle listener lighting keep them grounded.
Figures use their native imported pixels when they fit. Wide pairs are reduced
only enough to keep both full bodies visible with space between them. Closer
settings retain their previous upper-body framing.

Browse the complete roster at Drive > Career > Office > Cast Studio.
D-pad changes character, A or L/R changes outfit, X changes facing, Y compares
the previous portrait and B returns. Touch input remains unused. The original
story, relationships, progression, driving rules and version 7 saves are intact.
The fourteen concepts still have no story roles. Career remains a cutscene
prototype; this update does not add playable races.

Preview images distinguish indexed-art review sheets from actual emulator
captures. The indexed sheets show the exact imported palette and pixels;
the in-game captures show the tested ROM running in DeSmuME. Individual
256x384 screen captures are not enlarged or resampled.

- `cast-story.png` and `cast-drafts.png`: all 28 default facing pairs.
- `nia-poses.png`: all five of Nia's poses in both directions.
- `Story-Conversation.png`: an actual paired conversation in the diner.
- `Nia-In-Game.png`, `Nia-In-Game-Left.png`, `Mina-In-Game.png` and
  `Mina-In-Game-Left.png`: actual Cast Studio captures.

Validation on this exact ROM:

- 246,580,775 sanitized real-art staging checks across 2,145 pair configurations,
  all eight compatible floors and all 142 directional views. Speaker swaps keep
  positions and caches stable, and complete silhouettes fit across both LCDs.
- 4,632,846 sanitized Cast Studio checks across 213 views: 142 directional and
  71 previous portraits. Navigation, unchanged saves and cache restoration after
  returning to the story and both driving modes pass.
- Actual DeSmuME playback visits all 213 gallery views, ignores touch and returns
  to both driving modes. D01, D04, D05, D24 and S17 pass through 48 authored lines
  and 49 pages, including pagination and pause/resume, with zero archive errors.
- 38,934 sanitized campaign checks cover all 125 scenes, 923 messages and 929
  pages; 4,873 dialogue pixel checks cover clipping, lighting and the LCD seam.
- Independent alpha oracles and source/native visual reviews cover all 142
  additions. All 54 backgrounds, 76 preceding portrait payloads and 923 encoded
  message/shot records remain byte-identical to the published cast build.
- One background and two actor slots still require 302,592 bytes of the existing
  307,200-byte scratch lease. Main RAM and ITCM budgets pass.

The exact ROM hash and completed emulator reports are recorded in
`VALIDATION.json`. No phone, Delta or physical DS test was performed. The full
driving release matrix was not rerun for this focused art and staging update.
Later-story emulator saves are labelled manufactured review fixtures.

Reproduce after `make -C drive -j2` from the repository root:

    python3 drive/tests/quarter_assets.py --output drive/build/quarter-update
    python3 drive/tests/newquarter_staging.py --output drive/build/quarter-update
    python3 drive/tests/cast_gallery.py --output drive/build/quarter-update
    python3 drive/tests/story_assets.py --output drive/build/quarter-update
    python3 drive/tests/character_replacement.py --output drive/build/quarter-update
    python3 drive/tests/campaign.py --native-only --output drive/build/quarter-update
    python3 drive/tests/dialogue_pixels.py --output drive/build/quarter-update
    python3 drive/tests/continuous_story.py --output drive/build/quarter-update/continuous-story
