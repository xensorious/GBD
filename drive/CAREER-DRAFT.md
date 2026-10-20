# Rising Tide — playable story edition

Career now contains **125 playable dialogue scenes**, one per event in its five
class chapters. Finishing a scene records its completion and opens the next
node. Event 12 is the named rival encounter; event 25 closes the chapter and
opens the following class chapter. The final scene returns to Sunline Garage.

**There is still no racing gameplay in Career.** A race described by narration
is a story montage. Watching it does not simulate a driven race, award racing
cash, remove money, change vehicle statistics, or grant a separate set of parts.
Cars, installed upgrades, finishes, presets and cash remain the shared garage.

## Story and character arcs

You arrive at Nia's independent Sunline Garage when an ordinary lease renewal
has become difficult to answer. The club ladder offers paid work and racing
experience. As the crew progresses, a commercial team-name agreement becomes
an attractive but uncomfortable way to secure a place in the Coastline Cup.
The question is what the crew owes one another, not whether every businessman
is secretly a cheat.

Voss once lost his own garage and believes stable commercial backing protects
working people. His offer has real benefits and a real cost. Nia has to learn
that keeping Sunline independent does not mean making other people's choices
for them. Mina's paid photography and Jules's concern about reliable wages
make the crew's discussion more than a unanimous speech against sponsorship.
The solution is a readable independent guarantee supported by real club work.

| Class / chapter | Opening and ordinary work | Event 12 | Chapter final |
| --- | --- | --- | --- |
| D / Saltblock Quarter | Nia lends the small keys; Jules's banter becomes practical help; small heats pay actual workshop bills. | Jules's courier circuit turns a local rival into an ally. | Calder recommends Sunline to Lantern Mile. Nia chooses to keep the door open. |
| C / Lantern Mile | Marisol's harbor clubs reward reliability; Imani expects common sense; sponsorship begins as a conversation. | The shift-change duel earns Imani's respect and a place at the supplier's table. | Marisol backs the Foundry application and points out its team-identity clause. |
| B / Foundry Ring | Kade exposes a repeatable driving weakness; Rook explains the costs of a sporting application. | The clean rematch earns Kade's respect and the actual licensing rules. | Rook's club joins Calder and Marisol in guaranteeing an independent application. |
| A / Meridian Park | Ren teaches permanent-circuit procedures, sectors and longer sessions; the crew discusses Voss's offer honestly. | Ren's sector duel earns a sporting recommendation and an ongoing crew relationship. | Aster signs the independent Cup entry and distinguishes a license from a business contract. |
| S / Pelican Grand Prix | Sable asks for a real contest; ordinary sessions and paid work support the final series; Voss agrees to discuss actual terms. | Sable backs a fair independent-entry statement after the duel. | The final is a fair race in the story. Back at Sunline, the small-keys joke becomes a welcome for the next unknown driver. |

The complete screenplay is [story/rising_tide.txt](story/rising_tide.txt):
**628 authored messages**, including dialogue for twelve named characters,
the player and a narrator. It is also supplied as `STORY-SCRIPT.txt` in the ZIP.

### Setbacks that explain training

D.06 is a spontaneous, agreed exhibition with Kade. A narrated cutaway shows
the gap; Nia identifies untidy corner exits rather than blaming a magically
bad engine. D.07–D.11 become a short practice-and-income block before Jules.

B.02 is the benchmark exhibition. Kade wins through the way he links corners;
B.03–B.11 revisit approaches, exits, repeatability and patience before a fair
rematch at B.12. No scene steals parts, removes cash or penalizes the save.

Later preparation blocks have equally ordinary reasons: meeting a supplier,
funding entry costs, learning sectors, longer stints, and keeping the crew rested.
Not every event creates another conspiracy or another named enemy.

## Scene presentation

- **Face-to-face staging:** original, camera-facing character sprites over a
  painted location. The speaker comes forward; the other person recedes. The
  player is treated as a first-person viewpoint rather than a stock avatar.
- **Wide scene pans:** the camera eases across a wider car-meet illustration.
  A small speaker portrait sits beside the dialogue, leaving the scene visible.
- **Crew radio:** scenic views, caller portrait and a restrained animated radio
  waveform handle quick preparation conversations without another formal meeting.
- **Story montages:** letterboxed establishing shots and narration bridge the
  fictional exhibitions, duels and chapter finals while races are still absent.

There are six original backgrounds and twelve distinct transparent character
portraits. Soft cuts separate locations; camera motion eases between speakers;
portraits fade and subtly breathe; text types into a textured, retro teal panel.
Messages fit the screen without scrolling off the edge. Dialogue uses physical
buttons only. There are no touch targets.

## Progress, journal and replay

The map shows cleared, current and locked nodes. The next chapter opens only
after the preceding chapter's final scene. Earlier scenes can be replayed in any
order; a replay does not increase the saved frontier. Locked nodes can be inspected
on the map but cannot launch a scene. Returning to the map during a scene or
restarting it grants no completion. Holding A cannot cascade through messages.

Journal entries open as the relevant events are completed. Future chapter
outcomes are not exposed through an unlocked journal page. The Office now shows
the saved campaign record and a Continue shortcut. The old temporary progress
preview and its reset control have been removed.

The selected vehicle and its upgrades and cosmetics are not changed by scenes.
Story completion opens story events, not vehicle-purchase class gates. The
existing fleet-count requirements and shared economy remain in place.

## Controls

From Career's Route tab, choose the current or a cleared event and confirm.
A first reveals the remaining text, then advances to the next message.
B or Start opens a scene pause menu: Resume, Restart scene, or Return to map.
Up/Down selects a pause option. The completion card returns to the map.
In-game control explanations remain in the Controls page.

L/R still changes Career tabs. Left/Right changes the chapter or selected event.
The shared garage returns to Career with B.

## Saves and hardware

Version 7 adds one compact story frontier (0–125 completed events) to each
1000-byte profile. The three-profile bank remains an **8 KiB EEPROM**. Versions
1–6 import with their existing cars, money, parts, high-ID colors, histories and
presets preserved. Earlier builds cannot read version 7 records; keep a backup
when switching back to an older ROM.

The art and dialogue reside in an immutable NitroFS cartridge archive. Artwork
streams only when a shot or character needs it. The renderer leases the existing
small-sprite cache for its two background and two portrait buffers, then
invalidates the lease on return to ordinary rendering. No cache capacity is
removed from either driving mode, and the main executable stays below the ARM7
memory boundary. The ROM contains the archive; no separate files or SD setup are
needed in Delta.

ROM reads and EEPROM writes explicitly release the shared cartridge controller
between transactions. The immediate-scene autosave test closes the emulator,
starts a new process, and checks the first unlock before any driving takes place.

## Validation and remaining scope

Native sanitizer checks complete all 125 scenes through the actual state machine,
render all 628 messages, check sprite masks and screen guards, and verify locked,
replayed, held-input, aborted and chapter-boundary behavior. Codec tests round-trip
every frontier 0–125, reject invalid frontiers, import rich v6 records and preserve
three independent profiles.

The cartridge tour earns the first six events with button input, replays and
aborts scenes, checks ignored touch input, verifies fresh-process reloads, and
returns to both driving modes after the artwork lease. Later-chapter screenshots
use explicitly labelled native-codec fixtures; they are visual coverage, not a
claim of emulator-earned progression through those later chapters. Actual DS
emulator captures are distinguished from the original art atlases.

Career races, opponents, circuits, qualifying, weather, pits, entry fees and
race rewards are still future work. The narrative now supplies a concrete reason
and a staging point for each of those 125 future levels. Existing Free Roam
frame-time spikes remain a separately reported limitation; the story update does
not claim to resolve that engine issue. See `VALIDATION.json` for the measurements
on the packaged cartridge. Physical iPhone/Delta testing was not performed here.
