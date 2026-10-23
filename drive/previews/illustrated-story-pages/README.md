# Coastline: illustrated story pages

Open `Coastline.nds` directly in Delta. This is the complete game with all
artwork included; no extraction or separate asset files are required.

Levels D04–D06 now use three new finished paintings with Nia baked into the
environment. The 256x288 illustration extends across the top screen and the
upper half of the bottom screen. Only the lower 96 rows contain dialogue.
These pages have no overlaid character sprites, portrait badges, scene titles,
captions, cinematic bars or camera pans. Normal fades and pause remain.

- **D04 / Borrowed Time:** a poolside break leads into the garage lease problem
  and the decision to enter that night's heat.
- **D05 / After Hours:** recorded footage on the living-room television explains
  the spontaneous loss to Kade and the corner-exit habit that needs practice.
- **D06 / Wrong League:** the television is off; Nia and the player plan Calder's
  five short heats, ordinary paid work and the windscreen reference mark used
  in D07.

Each level has ten messages. The pool painting uses a relaxed upright pose
after the initial reference pose was rejected by image generation. The two
living-room paintings share the same setting, with driving footage visible
in the replay scene and a dark television during the planning conversation.
Source paintings and exact dialogue approvals are in
`art/story/baked-pages/catalog.json` in the repository.

`Illustrations-In-Game.png` shows all three actual DeSmuME screen captures.
The individual level images are unscaled 256x384 captures from this exact ROM.
Every illustration stays identical across its ten dialogue pages. The other
122 scenes retain their 893 encoded dialogue/shot records; all 218 portrait
payloads and the preceding 54 background payloads remain unchanged. The game
still has 125 levels, 923 messages and 929 dialogue pages. Existing version 7
saves, characters, relationships and driving mechanics remain compatible.
Previously completed levels can be replayed from the Career map.

Validation on this exact ROM:

- 13,496,183 sanitized plate checks cover all 30 new pages and all 73,728 native
  illustration pixels, zero actor loads, static framing, pause/resume, cold
  caches, return to legacy scenes and normal level unlocks.
- Actual DeSmuME playback covers D01, D03, D04, D05, D06, D24 and S17:
  70 messages and 71 pages, including pagination and pause/resume. All 30 new
  illustration areas remain pixel-identical as their dialogue advances.
- 39,599 campaign checks cover all 125 scenes and five staging methods;
  1,894 reply-continuity checks retain the original framing fixes.
- 4,390,873 gallery checks, 4,873 dialogue pixel checks and 247,173,511 standing
  checks preserve existing character sizing, poses, artwork and cache behavior.
- Three asset validators verify the new source hashes and reviewed native
  paintings, approved replacement scripts and preservation of every other
  scene and existing image. Main RAM, ITCM and the existing 302,592-byte scratch
  lease remain within budget.

`VALIDATION.json` records the exact ROM, source revision, images and reports.
No phone, Delta or physical DS test was performed. The complete driving release
matrix was not rerun for this focused story/art update. Later-story save
frontiers are explicitly manufactured review fixtures.

Reproduce after `make -C drive -j2` from the repository root:

    python3 drive/tests/plate_render.py --output drive/build/baked-pages-update
    python3 drive/tests/campaign.py --native-only --output drive/build/baked-pages-update
    python3 drive/tests/shot_continuity.py --prove-old-fails --output drive/build/baked-pages-update
    python3 drive/tests/continuous_story.py --output drive/build/baked-pages-update/story-scenes
    python3 drive/tests/story_assets.py --output drive/build/baked-pages-update
    python3 drive/tests/quarter_assets.py --output drive/build/baked-pages-update
    python3 drive/tests/character_replacement.py --output drive/build/baked-pages-update
    python3 drive/tests/cast_gallery.py --native-only --output drive/build/baked-pages-update
    python3 drive/tests/dialogue_pixels.py --output drive/build/baked-pages-update
    python3 drive/tests/newquarter_staging.py --output drive/build/baked-pages-update
