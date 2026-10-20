# Rising Tide: life between races

The campaign still has 125 event nodes and the same saved completion record.
This edition contains 923 messages, 54 panoramic backgrounds, fourteen named
people and fifty transparent portraits. Sixty-one scenes have expanded writing,
and the ending has an additional extension. Sixty-two events visit the new
settings; all 36 new backgrounds and all 50 portraits appear in the script.
Nineteen of the new backgrounds are used in only one event.

Career remains a cutscene prototype. Finishing a scene saves its completion and
opens the next node. Narrated results are fiction, not a racing simulation or
cash reward. Existing cars, upgrades, finishes and currency stay shared with
Endless Drive and Free Roam. Earlier v7 story saves retain their exact frontier;
completed nodes can be replayed to see the expanded scenes.

## The places have jobs in the story

| Place and events | What changes there |
| --- | --- |
| Mrs. Vale's greenhouse / D.03, D.20, B.24 | A neighbor who pays in oranges becomes an actual person. Repairs, ordinary favors and the clubs' guarantee acquire a community behind them. |
| Nia's kitchen / D.04, D.09, C.09, C.24 | Two streets from Sunline, she can admit that her father's sign, her own wishes and the rent are different questions. It is not a bedroom secretly added above the garage. |
| Eli's diner / D.05, D.21, C.16, B.13, S.15 | A recurring place to eat, mark a paid bill, welcome a rival and return between rounds. Eli has his own business; he isn't a free money source. |
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

Kade's D.06 and B.02 exhibitions remain scripted story defeats that lead into
specific practice blocks. They do not remove cash or parts. Jules, Imani, Kade,
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

The script has 393 authored shot cues. Each names a real visual subject and the
people physically present. A radio caller is not placed in the room. Camera
moves ease toward the named object and hold; there is no idle side-to-side loop
or speaker-number offset. Face conversations, directed inserts, radio and
letterboxed narrative shots remain mixed throughout the story.

The twelve principal characters have their original portraits plus casual,
work/race-day and reflective variants. Eli and Vale have their own original
portraits. Reflective variants retain the casual wardrobe, so a change of
expression does not also remove a racing suit in the same conversation. Image
extraction isolates the intended actor before normalization and indexed-color
conversion; stray pieces of adjacent atlas portraits must not appear above a
head or beside a shoulder.

All art is inside the ROM. Only two background buffers and two portrait buffers
are available to the scene at a time, sharing the same 256,000-byte scratch
lease as before. Additional paintings do not enlarge those buffers. Outfit and
expression choices use spare shot-record bits; the story runtime state and
8 KiB, three-profile EEPROM format stay unchanged.

## Verification

Native sanitizer checks complete all 125 scenes through the game state machine,
render all 923 messages, check text fit and screen guards, verify asset masks,
and exercise saves, replay, abort and camera pause/hold behavior. The archive
check compares every message, cast, outfit and camera target against the source,
requires all backgrounds and portraits to be used, and retains the RAM and ITCM
bounds. It also checks for large detached atlas fragments in encoded portraits.

The cartridge tour earns the first six events with controller input, verifies
immediate autosaves and three independent profile records, replays and aborts,
then returns to both driving modes. Later-chapter batteries are explicitly
labelled visual fixtures rather than claimed earned progression. Actual emulator
captures are used for the story previews. Catalog sheets are labelled indexed
asset previews and are not represented as emulator screenshots. Physical Delta
or DS testing is not claimed. Existing rare Free Roam frame delays remain a
separate, measured limitation in VALIDATION.json.
