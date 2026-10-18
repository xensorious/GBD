# Coastline: Endless Drive

A native Nintendo DS driving game for Delta, built as a first playable draft.
Drive a continuous four-lane highway from the city to the coast, across an ocean
causeway, through pine country and into red-rock canyons. Bank each run's score
and cash, then buy vehicles, tune them, change paint and choose a departure.

Import `Coastline.nds` into Delta's Nintendo DS library. Use a controller skin
that exposes both shoulder buttons and Start. No commercial ROM or patch is
needed. The game never uses touch input.

## Controls

| Button | During a run |
| --- | --- |
| R | Accelerate |
| L | Brake, then reverse; braking takes priority over acceleration |
| Left / Right | Steer continuously |
| Up + R | Hold Turbo for more acceleration and a higher top speed |
| Down + R | Hold Slow for gentler acceleration and a lower cruising speed |
| B | Hold the handbrake; steer while moving to drift |
| A | Sound the horn in cars; hold a wheelie on the motorcycle |
| Start | Pause; resume, bank the run, or review controls |

Slow takes priority if Up and Down are held together. Turbo and Slow are hold
modifiers, not switches. Menus use Up/Down and A/B. In Garage, Left/Right on the
vehicle row browses models; on Paint it previews colors, then A applies them.

The upper screen shows driving. The lower screen is a noninteractive dashboard
with speed and acceleration gauges, a mileage counter, points, pending cash,
armor and the current bonus multiplier. Every menu is on the lower screen;
the upper screen shows an animated drive or a vehicle in the showroom.

## Runs and rewards

Distance and active driving time earn consistent points. Road coins add points
and cash. Close overtakes earn a near-miss bonus once per traffic vehicle.
Maintaining full speed, a drift or a wheelie builds that bonus's multiplier
gradually to a maximum of **5x** over eight seconds. Breaking the action resets
its streak. The normal speed ceiling rises slowly during active driving,
eventually adding at most 50 km/h to the vehicle's normal ceiling.

Impacts damage armor and briefly reduce speed. Losing all armor ends the run.
The receipt lists the quantity and total points for every scoring category,
followed by the total points and cash earned. Press A to return to the main
menu. You can also bank a run from Pause. A run pays out once.

Only new forward distance earns mileage points; reversing does not create new
coin rows. Idling does not farm survival points. Coins are collected once.

The garage contains six vehicles, including a **standard motorcycle available
for cash with no challenge prerequisite**. Each owned vehicle has three levels
of engine, grip and armor upgrades. Factory, Coral and Lagoon paint are open
from the start. Other paint, the Sundown classic and several departures are
earned through the five road challenges. There are no microtransactions.

The city is the initial departure. Palm Coast is a cash purchase. Other starts
reward close overtakes, long-distance driving and drifting. All environments
can also appear naturally during a continuous run. Each region lasts about
7 km, with a gradual transition near its end.

## Saving

Cash, vehicles, tuning, paints, departures, records and challenge progress save
automatically after banking a run or making a garage/departure change. Delta
stores this as cartridge battery data. An unfinished run is not saved. Keep an
exported battery save when moving devices; save states include program state
and can depend on the exact ROM version. This game's save is separate from
Asterwild and Bramblebound.

## Build and test

```sh
cd /workspace/GBD
bash drive/tools/setup_nds.sh
python3 -m pip install -r requirements-dev.txt
make -C drive -j2
```

The setup reuses the local, pinned ARM GCC/newlib/libnds toolchain under
`/workspace/nds-tools`. No server or background process is needed. Original
checked-in artwork and the asset builder reproduce the game without an image
service. The output is `drive/build/Coastline.nds`.

```sh
bash drive/tests/setup_emulator.sh
make -C drive test
python3 drive/tools/package.py
```

Native sanitizer tests check driving, generation, scoring, shops, progression
and saves. The actual cartridge is exercised in headless DeSmuME using buttons
and read-only RAM observations. Screenshots, reports and test saves stay in
ignored `drive/build/`. The package step requires validation of the exact ROM.
Phone/Delta and physical DS testing must be reported separately from emulator
testing.

## First-draft scope

This is an endless arcade highway prototype with continuous steering and
illustrated 2.5D scenery, not a simulation or an open-world street network.
Roads curve and scenery changes, while four usable lanes remain continuous.
Traffic and pickups reuse fixed-size pools; road geometry and roadside props
are regenerated from a seed and distance without loading huge map files.
Handling, traffic density, rewards and upgrade prices are initial tuning values
for playtesting. The earlier games remain in their own folders and branches.
