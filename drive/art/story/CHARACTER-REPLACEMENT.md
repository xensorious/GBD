# Nia: installed character replacement

`nia-replacement.png` is the approved 1254x1254 RGBA five-pose source, copied
without regeneration from `exec-bb2ca7c9-76ab-4dee-8eff-d9230ce6839f.png`.
Its complete SHA-256 is pinned in the character replacement test fixture.
This is an original fictional adult character.

`story/cast.json` records individual extraction bounds. The sheet has three
poses above and two below; the fifth reaches into the empty sixth cell, so
equal-width grid extraction would clip the hand and wrench. The importer
discards saturated red/yellow export fringe, isolates the connected silhouette
and prepares a 256x264 indexed canvas directly from the approved source. The
lead portrait draws one source pixel per DS pixel, with aspect ratio preserved;
companions and dialogue thumbnails can shrink. There is no enlargement from
a smaller portrait export.

| Pose | Source bounds (left, top, right, bottom) |
| --- | --- |
| DEFAULT | 0, 0, 430, 627 |
| CASUAL | 430, 0, 836, 627 |
| GEAR | 836, 0, 1254, 627 |
| QUIET | 0, 627, 407, 1254 |
| ANNOYED | 400, 627, 900, 1254 |

The existing shared cast atlases remain unchanged. Overrides replace Nia's
four earlier expressions before archive IDs are assigned. All 51 existing
upper-body portraits are re-exported at the higher native resolution directly
from their original sources, preserving their design, wardrobe and expressions.

`nia-fullbody-approved-source.png` supplies the approved DEFAULT and CASUAL
standing poses. `nia-fullbody-expressions-source.png` extends the approved
GEAR, QUIET and ANNOYED gestures to complete standing figures. Extraction
bounds are in `story/cast.json`; `nia-fullbody.json` records expression-source
provenance, bounds and visual review. These five additions bring the archive
to 56 portrait records. They preserve Nia's approved face, hair, clothing,
tattoos and barefoot design, with complete hands, tools and feet.

Only Nia has full-body art. It is selected for FACE shots in the eight
floor-compatible settings listed in `tall-scenes.json`: GARAGE, MEET,
NIA_KITCHEN, DINER, GREENHOUSE, ROOFTOP, NIGHTGARAGE and EPILOGUE. Other cast
members and all remaining close settings retain upper-body portraits. The
512x324 standing backgrounds have real floor space and place Nia's feet at
world row 284. Original background source images remain unchanged.

The continuous scene is 256x288 across the top LCD and the upper half of the
bottom LCD. The lower 96 rows provide three dialogue rows. A native actor
canvas draws at 256x264; one background cache, two actor slots and their
palettes use 302,592 bytes of the existing 307,200-byte scratch pool.

The five-pose expression encoding fits the same eight-byte message header and
200-byte StoryLine. StoryState remains 488 bytes; no save migration is needed.
ANNOYED appears in D01's garage-keys exchange. Speaker, listener and transition
paths retain the five-pose encoding. The unchanged 923 messages
occupy 929 pages across 125 scenes; six messages split without changing shot,
speaker or cast until their final page. Pagination uses existing state padding.

Nia's only established blood relative is her unnamed father, who has no live
cast sprite. `nia-family-workshop.png` and `nia-family-lighthouse.png` update
the two actual family photographs, depicting the same father with short dark
brown hair and a fully clothed younger Nia in a blue T-shirt. Other incidental
prints are scenery and do not identify relatives. Calder is her father's
friend, Vale a customer/neighbor and Jules part of the crew. No named character
is established as her sibling, parent or romantic partner. Her name, dialogue,
plot and existing saved progression are preserved.
