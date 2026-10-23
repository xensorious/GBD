# Nia approvals, character wardrobes and illustrated pages

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

The original shared cast atlases remain unchanged as provenance. Nia's source
overrides still replace her earlier expressions before archive IDs are assigned.
The thirteen other story characters now use four newly illustrated variants each
from `restyle/catalog.json`: default, casual, work and reflective. The reflective
variant keeps the casual wardrobe. These active replacements provide distinct
styles and expressions without changing names or dialogue IDs. They are
prepared directly from their new source paintings at the same native resolution.

`nia-fullbody-approved-source.png` supplies the approved DEFAULT and CASUAL
standing poses. `nia-fullbody-expressions-source.png` extends the approved
GEAR, QUIET and ANNOYED gestures to complete standing figures. Extraction
bounds are in `story/cast.json`; `nia-fullbody.json` records expression-source
provenance, bounds and visual review. Nia's five upper-body and five full-body
imports retain their approved source and encoded pixels. They preserve her face, hair, clothing,
tattoos and barefoot design, with complete hands, tools and feet.

The earlier Nia-only full bodies remain available as original comparison art.
`quarter/catalog.json` adds separately authored LEFT and RIGHT three-quarter
full-body views for every current pose: five for Nia, four for each other story
character and one for each concept, in both directions (142 additions). Nia
retains her plain opaque white bra top, shorts, hair, jewelry and physical-side
tattoos; the tank-top reference does not replace her current outfit.

Standing FACE shots use the new art in the eight floor-compatible settings from
`tall-scenes.json`: GARAGE, MEET, NIA_KITCHEN, DINER, GREENHOUSE, ROOFTOP,
NIGHTGARAGE and EPILOGUE. Partners face inward in stable left/right places; close
settings retain upper-body portraits. The shared floor is world row 284, with a
four-pixel depth offset for the right partner. Original background source images
remain unchanged.

`new-cast/catalog.json` adds fourteen default full-body character studies.
They appear in Career → Office → Cast studio alongside the fourteen existing
characters, with concept labels and no story roles. The archive now contains
218 portraits: the original 57 upper-body records, five Nia full-body records,
fourteen concepts and 142 authored directional full bodies. These concepts do not
extend `story/cast.json`'s story actor list, so
YOU/NARRATOR IDs, dialogue encoding and saved progression remain unchanged.

The continuous scene is 256x288 across the top LCD and the upper half of the
bottom LCD. The lower 96 rows provide three dialogue rows. A native actor
canvas draws at 256x264; one background cache, two actor slots and their
palettes use 302,592 bytes of the existing 307,200-byte scratch pool.

The five-pose expression encoding fits the same eight-byte message header and
200-byte StoryLine. StoryState remains 488 bytes; no save migration is needed.
ANNOYED appears in D01's garage-keys exchange. Speaker, listener and transition
paths retain the five-pose encoding. The 125 scene IDs remain unchanged;
longer messages keep their shot, speaker and cast until their final page.
Pagination uses existing state padding.

`baked-pages/catalog.json` adds three scene paintings: NIA_POOL_EVENING for
D04, NIA_REPLAY_NIGHT for D05 and NIA_SOFA_PLAN for D06. They depict Nia by a
coastal pool, watching car footage on a living-room television, and planning
from a sofa. Nia is part of each painting rather than an imported actor layer.
The new `PLATE` method (`ST_PLATE = 4`) draws the native 256x288 central viewport
at x=128, y=18 in each 512x324 background. It continues through the top LCD's
192 rows and the bottom LCD's upper 96 rows, above the dialogue. No actor,
portrait badge, title, caption or letterbox bars cover these pages. Only
D04–D06 receive rewritten dialogue and staging; all other 122 encoded scenes
remain byte-identical.

Nia's only established blood relative is her unnamed father, who has no live
cast sprite. `nia-family-workshop.png` and `nia-family-lighthouse.png` update
the two actual family photographs, depicting the same father with short dark
brown hair and a fully clothed younger Nia in a blue T-shirt. Other incidental
prints are scenery and do not identify relatives. Calder is her father's
friend, Vale a customer/neighbor and Jules part of the crew. No named character
is established as her sibling, parent or romantic partner. Her name,
relationships and existing saved progression are preserved.

The wardrobe verification compares all ten encoded Nia portrait payloads and
the preceding 54 backgrounds with the earlier native archive, byte for byte.
All 66 wardrobe and concept portrait alpha oracles remain tied to their source
silhouettes. Nia's old approval pins remain independent of the wardrobe and
painted-page approvals. Cast IDs and relationships remain unchanged.

`tests/quarter_assets.py` checks that all 76 legacy portrait and 54 background
payloads remain byte-identical to the published cast build at commit `3cb4900`.
The fixture pins that ROM's full SHA-256. Independent alpha
oracles cover all 142 new imports; the source/crop approvals preserve the reviewed
artwork, and all 28 source/retained/discarded/ROM review pages have pinned hashes.
The archive now has 275 assets: 57 backgrounds and the same 218 portraits.
The three page paintings are appended, preserving all 54 earlier background
and all 218 portrait payloads. The baked-page allowance validates only D04–D06
as changed scenes and requires their approved `PLATE` sources. The asset count
and offsets use 32-bit fields, while
portrait IDs remain within 0–217. Left/right view selection does not mirror tattoos or other asymmetric
features at runtime. Cast Studio starts with the right-facing view; X changes
direction, Y compares the original art and L/R or A changes the pose.

`proportions.json` assigns each character a fixed uniform full-body scale across
all poses, directions and the studio. Nia is the native-size reference; Vale is
196/256 as large, retaining a smaller body and a comparable head size. Close
portrait framing remains separate. Compiled camera fits compare pose-union
alpha masks at their individual heights, preserving all opaque pixels without
forcing taller people down to the shorter person's pixel height. Player replies
retain the same authored on-screen cast and directions. The approved portrait
source art, indexed portrait payloads and individual height fixes are unchanged.
