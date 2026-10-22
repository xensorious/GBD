# Coastline: individual cast and concept roster

Open `Coastline.nds` in Delta. This is the complete, ready-to-import game;
no ZIP extraction or separate artwork folder is needed.

Thirteen existing characters now have individual wardrobes, silhouettes and
expressions, with four variants each: signature, off duty, on shift and
reflective. Nia retains her approved five close portraits and five full-body
poses. The other identities, relationships and all story dialogue are preserved.

Fourteen additional characters bring the named preview roster to 28. They are
unassigned concepts, with one full-body default each. They do not appear in
the story or alter progression. Browse everyone from
Drive > Career > Office > Cast Studio. The D-pad changes character; L/R or A
cycles outfits; B returns. Touch remains unused.

Existing version 7 saves use the same format. Both driving modes, vehicles,
customization and story progression are unchanged. Career remains the existing
cutscene prototype; this update adds character art and the preview gallery.

Preview files:

- Existing-Characters.png and New-Characters.png: default poses decoded from
  the actual indexed archive, with native character pixels. These are asset
  review sheets, not emulator screenshots.
- Outfits-and-Expressions.png: sixteen indexed views of Jules, Imani, Mina and
  Sable, showing their four distinct gestures and wardrobe variants.
- Story-Cast-In-Game.png and New-Cast-In-Game.png: actual DeSmuME captures of all
  28 defaults, laid out without scaling the individual 256x384 screen images.
- The Imani, Lexi and Orion images and Cast-Studio-Menu.png are unscaled
  256x384 emulator captures from this exact ROM.

Validation on the enclosed ROM:

- 2,202,587 ASan/UBSan gallery checks: all 28 identities and 71 pose views,
  direct indexed-pixel comparisons across both LCDs, navigation, frame guards,
  unchanged saved/story state and exact cache restoration after returning to
  story, Endless Drive and Free Roam.
- Actual fresh-profile DeSmuME tour of all 71 poses, passive touch, return to
  the Office and both driving modes; no emulated RAM writes or archive failures.
- 38,508 sanitized story checks across all 125 scenes, 923 messages and 929 pages;
  4,447 sanitized pixel checks for native drawing, clipping, lighting and seam.
- Actual DeSmuME playback of D01, D04, D05, D24 and S17: 48 lines / 49 pages.
  Later-scene batteries are explicitly manufactured review fixtures.
- All 66 new portrait imports match independently derived source alpha masks.
  All 54 backgrounds and ten Nia palette/pixel payloads remain byte-identical
  to the previous published build. Original story text and source art are pinned.
- One background and two actor cache slots still fit the same 307,200-byte
  shared lease (302,592 bytes required). The main-RAM and ITCM budgets pass.

VALIDATION.json contains the exact ROM, source revision, image hashes and
underlying reports. No phone, Delta or physical DS test was performed. The
complete driving release matrix was not rerun for this focused cast update.

Reproduce after `make -C drive -j2` from the repository root:

    python3 drive/tests/cast_gallery.py --output drive/build/cast-update
    python3 drive/tests/character_replacement.py --output drive/build/cast-update
    python3 drive/tests/story_assets.py --output drive/build/cast-update
    python3 drive/tests/dialogue_pixels.py --output drive/build/cast-update
    python3 drive/tests/campaign.py --native-only --output drive/build/cast-update
    python3 drive/tests/continuous_story.py --output drive/build/cast-update/story-scenes
