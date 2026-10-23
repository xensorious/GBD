# Rising Tide: life between races

The campaign still has 125 event nodes and the same saved completion record.
This edition contains 57 panoramic backgrounds and fourteen named story
characters. The 218 transparent portrait
records retain the preceding 76 illustrations and add 142 authored left/right
three-quarter full-body views for 71 available poses. The new studies are unassigned
concepts in Cast Studio, bringing its review roster to 28 without changing story
speaker IDs. D04–D06 now use three painted pages with Nia included in the artwork
and dialogue written for those settings. The other 122 encoded scenes remain
byte-identical, as do all existing portrait and background payloads.

Career remains a cutscene prototype. Finishing a scene saves its completion and
opens the next node. Narrated results are fiction, not a racing simulation or
cash reward. Existing cars, upgrades, finishes and currency stay shared with
Endless Drive and Free Roam. Earlier v7 story saves retain their exact frontier;
completed nodes can be replayed to see the expanded scenes.

## The places have jobs in the story

| Place and events | What changes there |
| --- | --- |
| Mrs. Vale's greenhouse / D.03, D.20, B.24 | A neighbor who pays in oranges becomes an actual person. Repairs, ordinary favors and the clubs' guarantee acquire a community behind them. |
| Coastal pool / D.04 | An evening conversation uses a complete painting of Nia beside the water. |
| Nia's living room / D.05, D.06 | Car footage on the television leads into a conversation and plan from the sofa. Nia belongs to each painted composition. |
| Nia's kitchen / D.09, C.09, C.24 | Two streets from Sunline, she can admit that her father's sign, her own wishes and the rent are different questions. It is not a bedroom secretly added above the garage. |
| Eli's diner / D.21, C.16, B.13, S.15 | A recurring place to eat, mark a paid bill, welcome a rival and return between rounds. Eli has his own business; he isn't a free money source. |
| Mina's bedroom / D.10 | Her results are still on a takeaway menu. The photographs matter beyond their usefulness to the racing crew, and she asks before publishing friends' pictures. |
| Jules's flat / D.17, B.08 | The invoices behind his jokes establish why a real payment date matters in the later contract debate. The second visit pays off a repaired delivery bike and a bill paid. |
| Sunline's roof / D.14, S.23 | The first borrowed chairs become a later conversation about meaningful stakes, wages, personal work and who gets to decide what Sunline is. |
| Record shop and print shop | Notices, a first notebook photograph, a small sign and independent paid work grow out of ordinary errands. |
| Lighthouse / D.24 only | Calder and Nia remember her father as a person, not an obligation to become a building. The photo and sea are intentional pan subjects. |
| Harbor bar, listening club and public washroom | Imani has work clothes, a need for an evening off and a practical sense of belonging. C.10's washroom conversation starts with empty soap and grease, not a random location change. |
| Ferry / B.07, S.17 | Kade and Sable can talk without a result sheet deciding who is allowed to speak. Both remain capable rivals. |
| Waiting room / C.19 only | Imani's mother's scheduled physio appointment is an ordinary commitment, not a sudden emergency invented to raise the stakes. The player learns not to demand a personal explanation. |
| Laundromat / B.16 only | Nia's response to Voss becomes a question she must put to the crew, not another speech about independence. |
| Voss's old workshop / B.20 only | The empty lift and last chair make his fear of unpaid wages concrete. His brother has another office; this is a complicated memory, not a new tragedy. |
| Train platform / B.19 only | Mina chooses paid photography on her own terms, with rights she checked herself. Her later season press pass is a further step, not a second first paid job. |
| Motel room and parking lot | Travel costs and the crew's own needs become visible. A.14's meeting happens in a budget room after Voss's sky-lounge offer. A.15 gives the answer against the actual Meridian budget. |
| Roadside washroom / A.06 only | A mirror, a nervous introduction and Jules's admission that jokes take rehearsal put the driver outside their comfortable role. |
| Press room, volunteer office and cafeteria | Work beyond driving has people behind it. Mina's pass, a borrowed notebook, dinner and ordinary permissions matter. |
| Locker room and empty stands | Ren's advice about rest has personal experience behind it. Sable can admit there are imperfect afternoons behind the champion's photograph. |
| Stewards' room / S.09 only | The video actually supports the track-limit correction. Honesty changes neither the rule nor the driver's right to belong. |
| Mina's exhibition / S.04, S.14 | Her own work has an audience. Selling a print doesn't mean selling the people in it; that gives the team-name debate a personal counterpoint. |
| Parked crew van / B.04, S.19 | A practical film review becomes a later quiet conversation about wanting somewhere to return after a bad result. The van is parked, not driven during the exchange. |
| New Sunline epilogue / S.25 only | The dawn composition returns to the keys and chairs with Eli and Vale present. The next driver is welcomed into a place populated by people, not just a trophy room. |

## Continuity retained

Kade's early exhibitions lead into specific practice blocks. These scripted
story results do not remove cash or parts. Jules, Imani, Kade,
Ren and Sable remain the event-12 rivals; Calder, Marisol, Rook, Aster and Voss
remain the chapter finals. The intended routes are closed street circuits for
D–B and permanent road courses for A–S.

The licensing thread keeps three things separate: Meridian's funded stage,
Aster's sporting invitation to the Cup, and the Cup's larger financial guarantee.
Voss's commercial offer is one route, with control of the team identity. The
independent club route must meet actual obligations too. S.20 publishes terms
without that name clause; S.21 clears the financial review. S.24 puts both
finalists under Aster's marshals. The result cannot change the agreed terms.

Nia learns that belonging does not entitle Sunline to everyone's evenings or
work. Jules's need for wages and Mina's independent photography are explicit
before the motel-room decision. Imani keeps ordinary family commitments. Kade
learns to meet people twice. The player finally says what they want beyond a
result. These developments connect the places instead of using new paintings
as interchangeable scenery.

Night finishes retain their night setting. The final moves from sunset to a
floodlit result and then dawn. The power-cut scene is an evening; Mina's early
weekend assignment precedes her season press pass. Nia's kitchen is separate
from the garage, and the roof is reached by an exterior ladder.

## Presentation and memory

One logical 256x288 scene spans the top screen's 192 rows and the bottom
screen's upper 96 rows. The lower 96 rows contain the speaker and three
dialogue rows. Longer messages split into pages without changing their text;
the speaker, cast, expressions and shot stay in place until the final page.
Pressing A reveals the current page, then advances its page or message. Pause and
resume retain the current page.

Authored shot cues name a real visual subject and the
people physically present. A radio caller is not placed in the room. Camera
moves ease toward the named object and hold; there is no idle side-to-side loop
or speaker-number offset. Face conversations, directed inserts, radio and
letterboxed narrative shots remain mixed throughout the story.

D04, D05 and D06 use the new `PLATE` staging method (`ST_PLATE = 4`), with
NIA_POOL_EVENING, NIA_REPLAY_NIGHT and NIA_SOFA_PLAN respectively. Their artwork
and source approvals are recorded in `art/story/baked-pages/catalog.json`.
Each 512x324 painting contains Nia and supplies its native 256x288 viewport at
x=128, y=18. The top LCD shows the first 192 rows and the lower LCD shows the
next 96 rows above the dialogue. The camera holds this composition without
actor sprites, portrait badges, titles, captions or letterbox bars.

Thirteen existing characters now have individually styled default, casual,
work/race-day and reflective variants, including new variants for Eli and Vale.
The styles follow each person rather than a shared team uniform: skate and Y2K
layers, punk workwear, romantic goth, varsity clothes, coastal glamour, techwear
and the older characters' own lived-in wardrobes. Nia's approved five upper-body
and five full-body poses remain unchanged, including the skeptical palm-up wrench
expression. Reflective variants retain each character's casual wardrobe. Image
extraction isolates the intended actor before normalization and indexed-color
conversion; stray pieces of adjacent atlas portraits must not appear above a
head or beside a shoulder. All actors are prepared on a native 256x264 canvas
directly from the source art, rather than enlarged from earlier exports.

Eight 512x324 replacement backgrounds provide standing floor space:
GARAGE, MEET, NIA_KITCHEN, DINER, GREENHOUSE, ROOFTOP, NIGHTGARAGE and EPILOGUE.
Standing story scenes now use inward-facing three-quarter full bodies for the
people present. Lower actor IDs keep the left place and higher IDs the right
place even when the speaker changes. A shared floor and small depth offset keep
both people grounded; listener lighting is restrained. Each character has a
fixed visual size from `art/story/proportions.json`.
Nia keeps a native 256x264 frame while Vale uses a proportional 196x202 frame;
heads stay comparable while Vale stands shorter. Pair cameras are fixed across
all poses and fit their actual silhouettes, with no opaque figure overlap.
Player replies preserve the authored visible cast, facing and held poses,
including conversations with three or four named people. Remaining close scenes
retain upper-body portraits. Cast Studio also displays the fourteen new studies, outside the story. Original
background sources remain intact; older close shots are fitted to the taller
view with their aspect ratios preserved. The workshop and lighthouse family
images retain their authored content and upper-body staging.

Career → Office → Cast studio is an additive review browser for all 28 people.
D-pad directions change the selected person, L/R or A cycles their available
variants. X changes facing, Y compares the original illustration, and B returns
to the Office. New studies are marked as concepts; no
story scenes, relationships, unlock requirements or rewards are attached to them.
Nia has five reviewed poses, the other existing characters have four, and each
new study has one. The browser leases and releases the same graphics storage as
dialogue, without changing the save format or story record.

All art is inside the ROM. One 512x324 background cache, two 256x264 actor
slots and their three palettes require 302,592 bytes of the existing
307,200-byte driving scratch lease. Extended expression choices retain the
eight-byte message header. Pagination uses existing StoryState tail padding;
the runtime state stays 488 bytes and the 8 KiB, three-profile EEPROM format
is unchanged.

Nia's unnamed father is her only established blood relative. The family photo
in the workshop and the keepsake at the lighthouse now depict the same
dark-haired father and fully clothed younger Nia. The D25 workshop insert pans
to the sign and family photo. Calder remains her father's friend; no other
named cast member is a parent, sibling or romantic partner. The replacement
changes no heritage references; none specify her appearance.

## Verification

The campaign suite traverses all 125 scenes and checks pagination, pause,
replay, abort, progress and saves. Dialogue-pixel and Cast Studio suites cover
the shared graphics cache, guarded drawing and browser controls. Standing and
player-reply regressions retain individual character heights, stable facing,
held poses and framing across speaker changes in the existing scenes.

`python3 drive/tests/plate_render.py`, run from the repository's parent, checks
the new native painting crop, LCD seam and dialogue boundary. It exercises the
three painted scenes through rendering, pause/replay, cache restoration and progression,
without adding portrait or shot overlays.

The asset validators pin all 218 existing portrait payloads and 54 preceding
background payloads. Only the three approved backgrounds are appended, for
57 backgrounds in total. The scene allowance permits rewritten encoded data
for D04–D06 only and requires their new `PLATE` artwork; the other 122 scenes
must remain byte-identical. Existing Nia approvals, portrait alpha oracles,
direction matrices and gallery bindings remain covered. Source/crop and native
palette/pixel hashes retain the reviewed art and individual proportions.

The [focused preview](previews/illustrated-story-pages/README.md) records the exact ROM
and underlying reports. The earlier broader cartridge tour earned the first six
events with controller input, verified autosaves and three independent profiles,
replayed and aborted, then returned to both driving modes. That full release
matrix was not rerun for this update. Later-scene batteries are labelled review
fixtures rather than earned progression, and indexed sheets are distinct from
actual emulator captures. Physical Delta or DS testing is not claimed.
Free Roam performance was measured separately; its optimization results are in
PERFORMANCE.json.
