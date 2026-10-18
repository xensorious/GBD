# Coastline: Endless Drive

A native Nintendo DS driving game for Delta, with a coastal glass interface,
ten vehicles and permanent vehicle mastery and driver progression.
Drive a continuous four-lane highway from the city to the coast, across an ocean
causeway, through pine country, red-rock canyons, a neon marina, mangrove
wetlands and citrus groves. Bank each run's score
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
| Start | Pause; resume, bank the run, review controls, or change settings |

Slow takes priority if Up and Down are held together. Turbo and Slow are hold
modifiers, not switches. Menus use Up/Down and A/B. In Garage, L/R switches
between Showroom, Parts and Setup. Left/Right on the vehicle row browses models;
on Paint or a setup row it previews a choice, then A applies it.

The upper screen shows driving from a lower chase camera, with a thin flat
horizon and a longer view of approaching traffic. The lower screen is a noninteractive dashboard
with speed and acceleration gauges, a mileage counter, points, pending cash,
armor and the current bonus multiplier. Every menu is on the lower screen;
the upper screen shows an animated drive or a vehicle in the showroom. Instrument
clusters change with the vehicle, including classic dials, digital sports
clusters, utility instruments and a motorcycle display. Miami, Sunset and
Midnight skins are available in Settings, along with independent music and
sound-effect toggles and volumes, road motion, camera shake and speed units.

## Runs and rewards

Distance and active driving time earn consistent points. Road coins add points
and cash. Close overtakes earn a near-miss bonus once per traffic vehicle.
Maintaining full speed, a drift or a wheelie builds that bonus's multiplier
gradually to a maximum of **5x** over eight seconds before career perks. Breaking the action resets
its streak. The normal speed ceiling rises slowly during active driving,
eventually adding at most 50 km/h to the vehicle's normal ceiling.

Impacts damage armor and briefly reduce speed. Losing all armor ends the run.
The receipt lists the quantity and total points for every scoring category,
followed by the total points and cash earned. Press A to return to the main
menu. You can also bank a run from Pause. A run pays out once.

Only new forward distance earns mileage points; reversing does not create new
coin rows. Idling does not farm survival points. Coins are collected once.

The garage contains ten vehicles, including a **standard motorcycle available
for cash with no challenge prerequisite**, a roadster, luxury sedan, sport
wagon and exotic. Factory, Coral and Lagoon paint are open from the start.
Other paint, the Sundown classic and several departures are earned through
the five road challenges. There are no microtransactions.

### Permanent progression

Cash and upgrades survive a crash. Each vehicle has its own mastery, earned
from new distance and driving bonuses when a run is banked. Six stages of
engine, grip and armor parts become available as mastery grows. Early parts
are affordable immediately; later parts reward time spent learning a vehicle.

Mastery also opens power, handling and chassis setups. These are free to switch
in the garage and have tradeoffs: acceleration against top speed, road grip
against slide handling, and lightness against impact protection. Parts improve
a vehicle; setups let you choose how it drives.

The same runs build shared career experience. Career levels award points for
three permanent perk paths: one emergency repair per run, faster combo growth,
or extra run cash. Each path has three ranks. Points can be reassigned for free
from Career, so a new vehicle can use a different build. The multiplier still
caps at 5x. Cash rewards, mastery and career experience are banked once.

The city is the initial departure. Palm Coast, Marina Nights and Citrus
Parkway are cash purchases. Mangrove Run opens at career level 2. Other starts
reward close overtakes, long-distance driving and drifting. All eight environments
can also appear naturally during a continuous run. Each region lasts about
7 km, with a gradual transition near its end.

## Saving

Cash, vehicles, parts, setups, mastery, career perks, paints, departures,
settings, records and challenge progress save automatically after banking a
run or making a menu change. First-draft cartridge battery saves migrate to
the expanded format, retaining their purchases and records. Delta
stores this as cartridge battery data. An unfinished run is not saved. Keep an
exported battery save when moving devices; save states include program state
and can depend on the exact ROM version. This game's save is separate from
Asterwild and Bramblebound.

If Delta lists this update as a separate game, export the original Coastline
save file and import it into the new entry through Delta's save-file controls.
The cartridge migrates that battery save on its next boot. Start the updated
game normally rather than restoring a save state made by the earlier ROM.

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
python3 drive/tests/scenery.py
python3 drive/tests/migration.py
python3 drive/tests/transition.py
python3 drive/tools/package.py
```

Native sanitizer tests check driving, generation, scoring, shops, progression
and saves. The actual cartridge is exercised in headless DeSmuME using buttons
and read-only RAM observations. Screenshots, reports and test saves stay in
ignored `drive/build/`. The package step requires validation of the exact ROM.
Phone/Delta and physical DS testing must be reported separately from emulator
testing.

## Scope

This is an endless arcade highway prototype with continuous steering and
illustrated 2.5D scenery, not a simulation or an open-world street network.
Roads curve and scenery changes, while four usable lanes remain continuous.
Traffic and pickups reuse fixed-size pools; road geometry and roadside props
are regenerated from a seed and distance without loading huge map files.
Road motion has three visual strengths; it does not change NPC cruising speed,
scoring distance or handling. Prefiltered scenery levels keep broad shapes
readable as perspective and speed change. Each roadside object keeps its identity
across a region transition instead of swapping into a different kind of prop.
Traffic follows safe gaps instead of driving through other cars.
Handling, rewards and progression remain subject to
playtesting. The earlier games remain in their own folders and branches.
