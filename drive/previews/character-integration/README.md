# Coastline: Nia character update

Import `Coastline.nds` from `Coastline-Character-Update.zip` into Delta.
The game artwork is inside the ROM; no separate asset folder is needed.

The approved five-pose character replaces all of Nia's previous portraits,
including the skeptical palm-up wrench pose. The lead is exported directly at
168x168 and drawn one source pixel per DS pixel. Her name, story, relationships
and saved progression remain unchanged. Her father is her only established
blood relative; both actual family photographs now match the new design.
Other named characters are not relatives, and their portrait pixels are intact.

This is a focused character-update build. The previous distribution bundle is
unchanged. Existing v7 saves use the same format and three-profile 8 KiB bank.

Validation on the exact ROM in this archive:

- 21,484 sanitized story checks across all 125 scenes / 923 messages, including
  fifth-pose speaker/listener transitions, saves and shared-buffer restoration.
- 1,424 sanitized pixel checks for native portraits, reduced companions,
  clipping, fades, lighting and two-portrait/two-background cache isolation.
- All five native transparency masks match the approved original crops exactly.
  All other actors' palettes/pixels, and all 52 unrelated backgrounds, match
  the previous build. The archive and resident-memory checks pass.
- Actual DeSmuME joypad completion of D01 and fresh-process reload of its save.
- Actual D24/D25 photo scenes reviewed in DeSmuME using explicitly manufactured
  later-scene review batteries; these are not claimed as earned progression.

Nia-Speaking.png, Nia-With-Mina.png and the two family-photo images are unscaled
256x384 emulator captures. Indexed-Portraits.png is an indexed asset review sheet,
not an emulator screenshot. VALIDATION.json contains exact hashes and reports.
The full driving/migration release matrix was not rerun. No phone, Delta or
physical DS test was performed.

Reproduce the automated checks from the repository root:

    make -C drive -j2
    python3 drive/tests/character_replacement.py --output drive/build/character-integration
    python3 drive/tests/dialogue_pixels.py --output drive/build/character-integration
    python3 drive/tests/story_assets.py --output drive/build/character-integration
    cd drive
    python3 tests/campaign.py --native-only --output build/character-integration
    python3 tests/campaign.py --emulator-only --case D01 --save-dir build/character-integration/new-review-save --output build/character-integration
