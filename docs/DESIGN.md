# Bramblebound: First Light

An original GBA creature RPG opening, ending at the first rival challenge. Target playtime is roughly 10–20 minutes for a new player, depending on exploration and reading; this is a design estimate, not a measured user-study result.

## The promise

Find an appealing partner, learn what makes it useful, and grow together without repeated preparation becoming the admission price to the adventure.

## What this opening includes

- Mossbell Commons: a settlement with a mentor, healer, gardener, local advice, a spring, and a north gate.
- Fernway Trail: visible wildlife, a free rest camp, optional supplies, a bridge, and a lookout.
- Three starter choices: Mossprig, Cindlet, and Rillip.
- Three wild species: Pebblit, Luma, and Burrbit. The nonselected starters are not catchable in this first area.
- Turn-based battles with energy, elemental matchups, predictable enemy intent, switching, items, recovery, and safe retreat from wild encounters.
- Deterministic recruitment: half health or a calming move, then one Trail Treat. An attempt that is not ready consumes nothing.
- Shared experience for traveling companions; no reward dependence on speed or battle rating.
- Level-three growth paths: STRIDER gives attack, energy, and BURST; WARDEN gives health, defense, and SHELTER. Paths are reversible for free at camp.
- An optional garden contribution. A helper appears in town and is unavailable in battle until recalled; the garden improvement persists without maintenance timers.
- A guide, objective journal, lead selection, following companion, music, sound effects, sound toggle, manual saving, milestone autosaves, and two validated SRAM records.
- A two-opponent rival duel, a Trail Pin ending, and continued exploration afterward.

## How the research affected decisions

The project research is in `/workspace/creature-catcher-findings.md` and its linked evidence ledgers. The implementation is an experiment with those findings, not proof that they are optimal.

1. The opening is an adventure with enduring companions. Mandatory death/rebirth was left out because a generational pet simulation promises a different experience.
2. Care sits at a natural pause: free camp rest and a visible town role. There are no hunger, illness, or toilet timers.
3. Species keep recognizable forms and traits while a small tactical branch changes how a favorite contributes. Paths do not reset levels.
4. The player chooses encounters. Recruitment rules are stated explicitly and do not consume supplies on an unready attempt.
5. Team experimentation is supported by shared experience, free recovery, and free path revisions. Helping in town is optional and reversible.
6. Battle rewards do not punish defensive teams for taking more turns. The rival's minimum-team playthrough is tested with every starter.
7. Loss returns the team to safety without lost friends, inventory, or money.

## First-draft boundaries

This is a complete playable opening, not a full-length RPG. There are no later regions, multiplayer, breeding, complex evolution trees, nickname input, money/shop economy, interiors, character creation, or separate difficulty modes. Growth paths demonstrate development rather than species evolution. There are six original species, of which four can join a single playthrough in this area.

The artwork, maps, dialogue, creature designs, font patterns, audio sequencing, and engine were created for this project. No commercial Pokémon/Digimon game code, maps, sprites, or music were used. The standard GBA boot header is required for cartridge compatibility. mGBA is a separate development/test dependency and is not bundled with the release.
