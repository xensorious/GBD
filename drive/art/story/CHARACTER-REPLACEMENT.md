# Nia: installed character replacement

`nia-replacement.png` is the approved 1254x1254 RGBA five-pose source, copied
without regeneration from `exec-bb2ca7c9-76ab-4dee-8eff-d9230ce6839f.png`.
Its complete SHA-256 is pinned in the character replacement test fixture.
This is an original fictional adult character.

`story/cast.json` records individual extraction bounds. The sheet has three
poses above and two below; the fifth reaches into the empty sixth cell, so
equal-width grid extraction would clip the hand and wrench. The importer
discards saturated red/yellow export fringe, isolates the connected silhouette
and prepares a 168x168 indexed canvas directly from the approved source. The
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
four earlier expressions before archive IDs are assigned; no unused old Nia
portraits remain in the ROM. Other actors' palettes and pixels are checked
against the preceding build by logical character and pose.

The five-pose expression encoding fits the same eight-byte message header and
200-byte StoryLine. StoryState remains 488 bytes; no save migration is needed.
ANNOYED is used in D01's garage-keys exchange and D05's diner conversation,
including speaker, listener and transition paths.

Nia's only established blood relative is her unnamed father, who has no live
cast sprite. `nia-family-workshop.png` and `nia-family-lighthouse.png` update
the two actual family photographs, depicting the same father with short dark
brown hair and a fully clothed younger Nia in a blue T-shirt. Other incidental
prints are scenery and do not identify relatives. Calder is her father's
friend, Vale a customer/neighbor and Jules part of the crew. No named character
is established as her sibling, parent or romantic partner. Her name, dialogue,
plot and existing saved progression are preserved.
