# Native story portrait build

Speaking portraits are exported directly from the existing character artwork
at 168x168 and drawn one source pixel per DS screen pixel. Their frame fills
the 168-pixel height below the scene header, preserving the figure aspect ratio.
Secondary characters and small dialogue thumbnails are reduced as needed.
Two backgrounds and two portraits fit in the existing 300 KiB driving cache;
this change adds no resident graphics buffer or save-format changes.

The custom replacement character sheets remain previews and are not installed.

Import Coastline.nds from Coastline-Native-Portraits.zip into Delta. This is a
focused portrait build; the previous distribution bundle is unchanged.
VALIDATION.json records the precise tested ROM hash and checks.

Validation:

- DS cartridge build and archive/header/memory checks pass.
- 1,421 sanitizer checks compare native and reduced drawing with an independent
  pixel oracle, including clipping, fades, light and protected cache slots.
- 19,694 sanitizer checks cover all 125 scenes, 923 messages and story/save rules.
- The actual cartridge completed D01 through joypad input in DeSmuME.
- Screenshots are unscaled 256x384 dual-screen emulator captures.

The full driving/migration release matrix was not rerun for this focused build.
No physical DS, Delta or phone test was performed.

Reproduce from the repository root:

    make -C drive -j2
    python3 drive/tests/dialogue_pixels.py
    python3 drive/tests/story_assets.py --output drive/build/native-portraits
    cd drive
    python3 tests/campaign.py --native-only --output build/native-portraits
    python3 tests/campaign.py --emulator-only --case D01 --save-dir build/native-portraits/fresh-save --output build/native-portraits
