# Coastline: consistent character sizes

Open `Coastline.nds` directly in Delta. This is the complete playable ROM,
with its artwork included. No extraction or separate assets are required.

Characters now keep their individual full-body sizes across outfits, expressions
and facings. Nia is the native-size reference. Mrs. Vale uses a proportional
196x202 frame beside Nia's 256x264 frame, so her body remains shorter and her
head stays comparable in size. The same character profiles apply in Cast Studio.
Scaling uses one factor for both axes; bodies are never widened or flattened.
Close portraits retain their separate camera framing.

Pair cameras remain fixed across every pose. They fit actual silhouette pixels
at the characters' different heights, allowing hands at different vertical
positions to share screen columns. Nia stays at full normal size beside Vale.
Wider pairs can use a shared camera reduction, preserving their relative sizes.
All opaque character pixels remain visible without figure overlap.

The two reported dialogue pages also exposed a separate framing bug: when the
off-camera player spoke, the listener lost their scene partner, switched facing
and moved to a full-size centered shot. All 95 paired player replies now retain
their authored visible cast, directions and held expressions. This also works
in shots with three or four named characters and during scene fades.

`Corrected-Scenes.png` shows actual emulator captures of Level 1 page 7 and
Level 3 page 4. `D01-Reply-Sequence.png` and `D03-Reply-Sequence.png` show the
adjacent pages side by side. Individual screen tiles are unscaled 256x384
captures from this exact ROM. On both reported replies, all 73,728 illustration
pixels match the preceding page while the dialogue updates.

The approved source artwork, all 218 indexed portrait payloads, all 54
backgrounds and all 923 encoded dialogue/shot records are unchanged. The story,
relationships, driving rules and version 7 save format remain intact.

Validation on this exact ROM:

- 247,173,511 sanitized standing checks: 2,145 real-art configurations on eight
  floors, 2,054 pose-stability comparisons across all 91 story pairs, all 218
  pose envelopes, no opaque figure overlap, and Nia/Vale's individual sizes.
- 1,930 sanitized player-reply checks: reported pages, larger groups, held
  skeptical poses, cold caches, pause/replay/abort and previous-shot fades.
  The preceding published renderer fails the exact D01 visual regression.
- 4,390,873 sanitized gallery checks across all 213 views, including uniformly
  scaled indexed pixels, unchanged saves and cache restoration after returning
  to the story and both driving modes.
- Actual DeSmuME playback of all 213 gallery views and six story scenes:
  D01, D03, D04, D05, D24 and S17. All 60 messages and 61 pages pass, including
  pagination, pause/resume, passive touch and both driving-mode returns.
- 38,934 campaign checks, 4,873 dialogue pixel checks, independent source alpha
  and payload checks, and the unchanged scratch lease pass. Main RAM and ITCM
  remain within budget.

`VALIDATION.json` records the exact ROM, source revision, image hashes and
reports. No phone, Delta or physical DS test was performed. The full driving
release matrix was not rerun for this focused visual fix. Later-story saves
are explicitly manufactured review fixtures.

Reproduce after `make -C drive -j2` from the repository root:

    python3 drive/tests/newquarter_staging.py --output drive/build/proportions-update
    python3 drive/tests/shot_continuity.py --prove-old-fails --output drive/build/proportions-update
    python3 drive/tests/cast_gallery.py --output drive/build/proportions-update/gallery
    python3 drive/tests/continuous_story.py --output drive/build/proportions-update/final-story
    python3 drive/tests/campaign.py --native-only --output drive/build/proportions-update
    python3 drive/tests/dialogue_pixels.py --output drive/build/proportions-update
    python3 drive/tests/story_assets.py --output drive/build/proportions-update
    python3 drive/tests/quarter_assets.py --output drive/build/proportions-update
    python3 drive/tests/character_replacement.py --output drive/build/proportions-update
