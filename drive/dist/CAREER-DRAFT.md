# Rising Tide — Career interface draft

This build adds a browsable campaign proposal, **not playable races**. Drive has a third illustrated Career card. All cars, installed parts, paint, liveries and departure choices belong to the existing shared garage. The Home tile previously called Career is now called **Driver**, retaining its existing XP and perks.

## Campaign spine

You and Nia run an independent neighborhood garage. Club nights pay the bills; the Coastline Cup offers a ladder out. Promoter Voss wants to buy the garage's name and control its entry. The crew moves from street clubs to licensed road courses while keeping its independence. Rivals can become allies; bosses represent sporting milestones rather than five unrelated villains.

| Class | Chapter | Venue | Event 12 | Event 25 |
| --- | --- | --- | --- | --- |
| D | Saltblock Quarter | Closed neighborhood street circuits | Jules / The Courier | Calder / Block Captain |
| C | Lantern Mile | Closed waterfront and night circuits | Imani / Night Shift | Marisol / Harbor Queen |
| B | Foundry Ring | Closed industrial street circuits | Kade / The Benchmark | Rook / Iron Crown |
| A | Meridian Park | Permanent road courses | Ren / Sector Ghost | Aster / The Gatekeeper |
| S | Pelican Grand Prix | Permanent Grand Prix road courses | Sable / Silver Apex | Voss / The Promoter |

There are 25 titled events per chapter, 125 total. Each chapter has five journal blocks: opening, practice/cash-building, the event-12 duel, preparation for the final, and event 25. Regular races can simply be the crew working and training; not every race must advance a conspiracy.

D event 6 proposes a clearly signposted story exhibition against a much faster visitor, followed by a practice block. B opens with Kade exposing a racecraft gap before the event-12 rematch. These are written concepts only. Neither scene runs, charges money nor forces a gameplay loss in this build. When implemented, avoid invalidating an unexpectedly successful player drive: use a separate scene or acknowledge the player's result.

## Screens

The top display contains a compact illustrated city atlas with a stable, irregularly scattered 25-node route. Dotted connections show event order. Gold indicates the current event, green indicates a cleared event, and hollow gray indicates locked events. A white ring indicates the event being inspected. Event 12 has a larger marker and event 25 a diamond. A/S chapters add a circuit footprint and paddock to the atlas. This is a campaign illustration, not a traversable map or finalized race geometry.

The lower screen retains the established interface layout and all six palette options:

- **Route:** chapter, event, race briefing and shared garage. Briefings have objective, story-context and entry-plan pages. Cash figures are explicitly proposed, not balanced or awarded. Higher-class chapters and locked events are browsable for design review.
- **Story:** five journal blocks, Nia's garage-radio commentary and workshop notes.
- **Rivals:** miniboss and boss dossiers, driving strengths and fair-race ideas. Portraits are original placeholder art.
- **Plans:** proposed systems listed below, each with fit and shared-garage notes.
- **Office:** shared garage shortcut, a temporary map-state preview, reset and scope notes.

The Office preview cycles zero through 25 completed nodes for the selected chapter. It exists to inspect the three map states. It is labelled **PREVIEW ONLY / NOT SAVED**, never awards money, changes unlocks or records victories, and resets on a fresh boot. It is not an implemented progression or debug cheat for racing.

## Proposed systems to edit next

1. **Race licenses:** sporting invitations for event entry, separate from the existing vehicle-purchase fleet-count requirements. Do not charge twice to use an already purchased car.
2. **Sponsor offers:** optional clean-finish or podium clauses for bonus cash. No sponsor-exclusive parts or cosmetics and no separate currency.
3. **Practice and ghosts:** free track practice, personal-best ghosts and suggested target laps. Training should remain available when the player is short of cash.
4. **Race-day choices:** short club events versus longer cash-building circuits for D–B. Qualifying, a weather forecast and a simple pit window for A/S. Road courses can feel more strategic without replacing the steering system.
5. **Crew reputation:** dialogue and invitations acknowledge helping rivals and finishing cleanly. Reputation is not a duplicate upgrade tree or customization shop.

Possible later mission variations include time trials, sector targets, clean races, endurance races and optional sponsor clauses. Traffic stays outside closed racing routes. Rivals should have readable strengths, use understandable vehicle rules, and avoid invisible catch-up advantages.

## What remains unbuilt

Race tracks, start grids, opponents, laps, qualifying, event eligibility, rewards, persistent campaign progression, boss gameplay, weather and pit mechanics are not implemented. The preview deliberately cannot launch a race. Race lengths, payouts, balancing, repeatable income and difficulty need a later design pass. The existing Endless Drive and Free Roam modes remain playable.

Save schema remains version 6. Career browsing adds only transient UI state; existing profiles and their three-slot bank retain their format.

## Navigation

No touchscreen input is used. From Drive, Down selects Career; A opens it. L/R changes tabs, Up/Down changes rows, Left/Right changes the selected chapter/event or preview count. A opens a reading page; A or directional buttons cycles its three pages. B closes a page, then returns to Drive. Opening the shared garage from Career returns to Career with B. In-game control explanations stay in the Controls page.

## Validation

`tests/campaign.c` checks all 125 event names and coordinates, all marker states, navigation and wraparound, six themes, screen bounds and unchanged save layout under AddressSanitizer/UndefinedBehaviorSanitizer. `tests/campaign.py` performs an actual cartridge tour with joypad input and read-only RAM observations, including ignored touch input, all chapters, story/rival/mechanic pages, temporary map progression, shared-garage return, existing-mode launch and a fresh-process reload. Funded fixtures used by other regression tests are not evidence of earned Career progression.

### Known performance issue in this review build

Functional, save, UI and shadow-position checks pass. The extended Free Roam
audit **does not pass its strict timing gate**: 12 of 4,065 measured display
intervals lasted 66.7 ms (median 33.3 ms, 95th percentile 50 ms; approximately
28 updates/second over that route). The two shorter vehicle routes pass.
This is retained as a known regression from the previous release, rather than
claiming the performance audit passed. The separate Career renderer preserves
the driving renderer's code, but the larger binary still changes timing.
Further engine optimization is deferred from this UI foundation task.

The package explicitly records this review-build exception in VALIDATION.json
and PLAY.txt. `tests/shadow_audit.py` retains its original strict assertion;
ordinary packaging still refuses timing failures. The review bundle uses the
explicit `--allow-ui-preview-timing-regression` packaging flag.
