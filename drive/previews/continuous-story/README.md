# Coastline: continuous story scenes

Import `Coastline.nds` from `Coastline-Continuous-Story.zip` into Delta.
All artwork is included in the ROM; no separate asset folder is required.

The cutscenes now span the full top screen and the upper half of the bottom
screen, forming a continuous 256×288 illustration. Dialogue occupies the
remaining 96 pixels. The physical DS screens retain their normal resolution.

Five full-body Nia poses use the approved character design. Eight environments
have new standing-height paintings with foreground floor space. Nia uses full
bodies in these scenes and retains her existing upper-body poses for close
camera angles. Only Nia has new character artwork; the other cast members keep
their original upper-body art. Speaking portraits are exported directly from
the source paintings at a native 256×264 canvas and drawn one-to-one; small
secondary portraits remain reduced.

Six messages require a second page. All 923 messages are preserved across 929
pages, and a page flip preserves its speaker, illustration and authored shot.
The pause and completion panels also fit the lower half of the bottom screen.

The story, names, relationships and save format are unchanged. Existing v7
three-profile saves remain compatible. This is a focused story-layout preview;
the previous distribution and driving mechanics are unchanged.

Validation on the exact enclosed ROM:

- 38,448 ASan/UBSan campaign checks across all 125 scenes and 929 pages: complete
  text coverage, font wrapping, page input, progression, migration and buffers.
- 4,387 ASan/UBSan pixel checks: native rectangular portraits, seam continuity,
  clipping, lighting and protection of the lower dialogue area.
- Archive dimensions, transparency, maps, source preservation and memory checks.
  Story rendering uses 302,592 bytes of the existing 307,200-byte cache lease.
- Actual DeSmuME runs of D01, D04, D05, D24 and S17: 48 authored lines and 49
  pages, including an actual two-page message and pause/resume. Twelve captures
  received visual review. A separate indexed-image framing review checks 29
  named subjects across 14 events. Later-scene save fixtures are explicitly manufactured
  review data, not claimed as earned progression. Emulated RAM was read only.

The six PNG previews are unscaled 256×384 emulator captures. The two dialogue
images show successive pages of the same message and shot. VALIDATION.json
contains exact hashes and the underlying reports. No Delta, phone or physical
DS test was performed; the full driving release matrix was not rerun.

Reproduce from the repository root after `make -C drive -j2`:

    python3 drive/tests/character_replacement.py --output drive/build/continuous-story
    python3 drive/tests/story_assets.py --output drive/build/continuous-story
    python3 drive/tests/dialogue_pixels.py --output drive/build/continuous-story
    python3 drive/tests/campaign.py --native-only --output drive/build/continuous-story
    python3 drive/tests/continuous_story.py --output drive/build/continuous-story
