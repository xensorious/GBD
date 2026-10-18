# Coastline: Endless Drive

An original **Nintendo DS** procedural highway driving game for Delta.
Drive from the city to tropical coastlines, an ocean causeway, pine forests,
red-rock canyons, a neon marina, mangrove wetlands and citrus groves.
Earn points and cash, bank an itemized run receipt, and return
to the garage to buy, tune and paint your vehicles.

- [Download the DS cartridge](drive/dist/Coastline.nds)
- [Download the complete ZIP](drive/dist/Coastline-DS.zip)
- [Watch actual cartridge gameplay](drive/dist/highway-motion.gif)
- [See the turbo dashboard and guide](drive/dist/Turbo.png)
- [See grounded roadside and water scenery](drive/dist/Grounding.png)
- [Controls, progression and build instructions](drive/README.md)

![Actual Nintendo DS screens](drive/dist/Preview.png)

R accelerates, L brakes/reverses, Left/Right steer, Up adds Turbo, Down adds
Slow, B is the handbrake, and A is the horn or motorcycle wheelie. All driving
modifiers are held. Start pauses. All menus use buttons on the lower screen;
while driving, that screen becomes a noninteractive instrument dashboard.
Touch input is ignored throughout.

The driving dashboard puts health, combo, points, cash and the new turbo reserve
above the gauges. Turbo lasts eight seconds from full and refills through near
misses, controlled drifts, motorcycle wheelies and Slow recovery. A 5x score
combo gives 2x skill recharge. The Turbo Guide explains the rates in game.

Roadside buildings now face inward from projected foundations, trees and lamps
use measured ground contacts, and boats and piers sit on the water plane.
Overhead signs and the awkward bridge sign frames are removed.

This version contains ten vehicles, eight regions, seeded traffic and road
coins, close-overtake bonuses, capped speed/drift/wheelie streaks, six-stage
parts, vehicle mastery, handling setups and shared career perks. Glass menus,
vehicle-specific dashboard designs and three interface skins accompany
independent music/SFX settings. Purchases and progression persist in cartridge
battery saves, including migration from the first draft. The standard motorcycle
is a cash purchase without a challenge requirement. Special gear, paint and
locations use challenge rewards.

![Eight environments and the motorcycle](drive/dist/Locations.png)

Scenery captures use a documented pre-unlocked battery-save fixture for visual
review. The main validation plays a fresh profile and checks normal earnings,
purchases and save reloads. No emulator RAM writes or save-state restores are used.

## Build and validate

```sh
bash drive/tools/setup_nds.sh
make -C drive -j2
bash drive/tests/setup_emulator.sh
make -C drive test
python3 drive/tests/scenery.py
python3 drive/tests/migration.py
python3 drive/tests/transition.py
python3 drive/tools/package.py
```

The game, original artwork, procedural generator and test harness live in
`drive/`. No web server is needed. The build creates `drive/build/Coastline.nds`;
generated assets and emulator artifacts stay in ignored `drive/build/`.
Tests execute native rules with sanitizers and the real ARM9/ARM7 cartridge in
DeSmuME. Phone/Delta and physical hardware testing are separate.

The earlier Asterwild DS game remains in [nds/](nds/README.md), and the original
Bramblebound GBA experiments remain in `src/`, `tools/` and `dist/` and their
release branches. Coastline has its own gameplay and save format.
