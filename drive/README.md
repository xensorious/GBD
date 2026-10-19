# Coastline: Endless Drive

A native Nintendo DS driving game for Delta, with four complete interfaces,
31 vehicles, five performance classes and permanent vehicle mastery and driver progression.
Drive a continuous four-lane highway from the city to the coast, across an ocean
causeway, through pine country, red-rock canyons, a neon marina, mangrove
wetlands and citrus groves, then into a neon metropolis, a deep jungle,
a ghostly bayou, an aurora coast and a luminous saltway. Bank each run's score
and cash, complete optional dispatch jobs, then buy vehicles, tune them,
change their appearance and choose a departure.

Import `Coastline.nds` into Delta's Nintendo DS library. Use a controller skin
that exposes both shoulder buttons and Start. No commercial ROM or patch is
needed. The game never uses touch input.

## Opening and profiles

The opening drive spans both LCDs without stretching the car sprites. The top
screen has a broad sky with clouds and birds above the distant road, and the
car drives on the lower screen. Press any physical button to open the three
save slots on the upper screen. Up/Down chooses a slot, A or Start opens it,
and B returns to the movie. Touch never starts the game or selects a slot.

Selecting a profile slides the lower driving view onto the upper screen while
the existing main menu rises into the lower screen. The camera eases into the
normal menu preview. Each slot keeps its own cash, fleet, parts, mastery,
cosmetics, presets, records, departures and settings. Selecting an empty slot
starts a new profile. Existing slots continue their saved progress.

Press B on the main menu to return to the save selector. Earlier single-profile
battery files are accepted as **Save 1**. Saves 2 and 3 start empty. The complete
three-profile bank uses an **8 KiB EEPROM**, with three independently checked
512-byte records; each profile retains the existing packed 500-byte format.

## Controls

| Button | During a run |
| --- | --- |
| R | Accelerate |
| L | Brake, then reverse; braking takes priority over acceleration |
| Left / Right | Steer continuously |
| Up + R | Hold Turbo to spend reserve for more acceleration and a higher top speed |
| Down + R | Hold Slow for gentler acceleration and a lower cruising speed |
| B | Hold and steer for a controlled drift; release to straighten |
| A | Sound the horn; hold a wheelie on motorcycle, bicycle or moped |
| Start | Pause; resume, bank the run, review controls, or change settings |

Slow takes priority if Up and Down are held together. Turbo and Slow are hold
modifiers, not switches. Menus use Up/Down and A/B. In Garage, L/R switches
between Showroom, Tuning, Style and Dealer. Left/Right on Grab your keys in Showroom
browses owned models. In Dealer, Left/Right or Select changes class; Up/Down
highlights cars and A purchases one. Style and build choices are previews until
you choose their Install row. B returns to the previous menu.

The upper screen shows driving from a lower chase camera, with a thin flat
horizon and a longer view of approaching traffic. The lower screen is a noninteractive
dashboard. Health, combo, points and pending cash sit at the top, followed by
the turbo reserve, mileage and coins. Speed and acceleration instruments sit
underneath that information. Every menu is on the lower screen;
the upper screen shows an animated drive or a vehicle in a departure-themed garage. Instrument
clusters change with the vehicle, including classic dials, digital sports
clusters, utility instruments and a motorcycle display. Settings offers
**Fusion** as the standard midnight glass interface, **Pearl** with inset ivory
ceramic tiles and brass details, and **Voltage** with illuminated cut-corner
frames and electric foil. All three share the same layout: all four D-pad
directions move between home tiles, L/R cycles the Audio, Display and Guide
pages in Settings, Up/Down scrolls through the full list, and Left/Right adjusts
the selected value. Their artwork continues through every menu and the garage.
The animated upper-screen drive uses compact translucent floating captions,
leaving the skyline, scenery and road visible.
Settings also includes independent music and
sound-effect toggles and volumes, road motion, camera shake and speed units.

### Turbo reserve

Each run begins with eight seconds of boost. A new boost needs at least 10%
reserve. When the tank empties, release Up before boosting again. R continues
to accelerate normally when reserve is empty. The meter shows the remaining
percentage, boost time and recharge state.

| Clean driving action | Base recharge |
| --- | --- |
| Near miss | 10% immediately |
| Controlled drift held for at least 0.75 seconds | 4% per second |
| Motorcycle wheelie held for at least one second | 3% per second |
| Slow mode, Down + R, above 40 km/h | 2% per second |

Near-miss, drift and wheelie recharge grows with the score combo, reaching
**2x recharge at a 5x combo**. Slow recovery stays at its fixed rate. Continuous
sources use the strongest qualifying action rather than stacking. Recharge
stops during active boost, impacts, reverse travel and wall grinding. Ordinary
straight-line acceleration, idling and coins do not refill the tank.

Open Controls from Settings or Pause and use L/R to switch between the button
guide and the Turbo Guide. All guide pages use buttons.

## Runs and rewards

Distance and active driving time earn consistent points. Road coins add points
and cash. Close overtakes earn a near-miss bonus once per traffic vehicle.
Multipliers advance in exact **0.25x steps**, from 1.00x to a maximum of **5.00x**.
Without career perks, a full-speed streak takes **48 seconds** to reach 5x,
earning one quarter-step every three seconds. Drift and wheelie streaks take
24 seconds, with one quarter-step every 1.5 seconds. Focus perks shorten those
times by up to 45%. Breaking an action resets its streak.
The normal speed ceiling rises slowly during active driving,
eventually adding at most 50 km/h to the vehicle's normal ceiling.

Hold B while steering to assist a turn with a visible drift angle. Steering
responds promptly, countersteering changes direction, and releasing the drift
quickly settles the car. Grip and handling setups still affect its response.

Police follow the run. After a three-second launch grace, remaining stopped or
reversing for about three seconds lets them catch you. The dashboard shows the
approaching capture and its countdown. Moving forward reduces the pressure;
Slow mode remains safe, and pausing freezes the pursuit.

Impacts damage armor and briefly reduce speed. Losing all armor also ends the
run. The results screen resembles a traffic citation, itemizing the quantity
and points for every bonus. Its cash award is **added to your wallet**. Being
caught, crashing, and banking manually have distinct explanations. Press A to
return to the main menu. You can bank a run from Pause. Each run pays out once.

Only new forward distance earns mileage points; reversing does not create new
coin rows. Idling does not farm survival points. Coins are collected once.

The garage contains **31 vehicles**. Each of the five classes includes a value
option, an all-rounder, and specialists in top speed, acceleration and armor.
The standard motorcycle remains a cash purchase without a driving challenge.
Roller skates, a bicycle, a moped, a Segway, a skateboard and a horse are
playable rides using the same driving controls and progression.

| Class | Total vehicles needed to purchase |
| --- | --- |
| D | 0 |
| C | 2 |
| B | 5 |
| A | 8 |
| S | 10 |

These counts include all owned vehicles, including novelty rides. Existing
purchases remain available after updating. Higher classes cost more and offer
stronger factory performance. Dealer separates D, C, B, A and S into ordered model lists.
Factory, Coral and Lagoon paint are open from the start.
Other paint, the Sundown classic and several departures are earned through
the eight road challenges. There are no microtransactions.

### Permanent progression

Cash and upgrades survive a crash. Each vehicle has its own mastery, earned
from new distance and driving bonuses when a run is banked. Six stages of
engine, grip, armor and turbo-recovery parts become available as mastery grows.
Turbo recovery improves recharge by 5% per stage. Early parts
are affordable immediately; later parts reward time spent learning a vehicle.

Tuning offers free, mastery-gated **Balanced, Acceleration, Top speed, Heavy
and Drift** builds alongside the four permanent upgrade tracks. Earlier mixed
setups remain installed as Custom until you choose a named build.

### Garage studio

Four tabs occupy the very top of the lower screen, with your wallet and fleet
count just below them. The top screen displays the highlighted car inside a
three-wall workshop whose floor, lighting and window scenery match your departure.

- **Showroom:** browse owned vehicles and **Grab your keys** to equip one.
  Left/Right browses your collection; A equips the highlighted vehicle.
  Open **Departure** to browse, unlock, purchase and select locations in one
  place. The upper screen shows the highlighted vehicle, class and stats;
  Tuning and Style work on that vehicle without a duplicate selection card.
  Three saved presets remember a vehicle, installed build, finish and departure.
  Load preset shows the refitting cost before you apply it. Permanent upgrades
  belong to the vehicle and are not copied or reset by presets.
- **Tuning:** choose a build and install engine, road grip, armor and turbo-recovery parts.
- **Style:** preview paint, livery, tire/hardware finish and five smoke colors.
  Install finish applies every pending change together or leaves everything
  unchanged if you cannot afford the total. Cosmetics never change performance.
- **Dealer:** select a class, browse its model list, then buy a car. Owned cars
  lead back to Showroom for equipping.

A cosmetic installation is a paid service, not a permanently free inventory item.
Switching back to a previously installed finish costs **50% of its normal price**,
rounded up, per vehicle. Leaving the currently fitted finish in place costs nothing.
Factory restoration is also a paid discounted installation. Presets follow these
same prices, so they do not bypass cosmetic costs. Existing purchased livery and
wheel history remains eligible for the discount after updating.

Dispatch offers three optional driving contracts. Choose an objective before
a run, such as collecting coins, drifting or making close passes. Completing
the selected objective and banking the run awards an additional cash payment.
A contract pays once; later offers provide new goals. Contracts are available
offline and do not depend on a daily reset.

The same runs build shared career experience. Career levels award points for
three permanent perk paths: one emergency repair per run, faster combo growth,
or extra run cash. Each path has three ranks. Points can be reassigned for free
from Career, so a new vehicle can use a different build. The multiplier still
caps at 5x. Cash rewards, mastery and career experience are banked once.

The city is the initial departure. Palm Coast, Marina Nights and Citrus
Parkway are cash purchases. Mangrove Run opens at career level 2. Other starts
reward close overtakes, long-distance driving and drifting. All thirteen environments
can also appear naturally during a continuous run. Each region lasts about
7 km, with a gradual transition near its end.

## Saving

Cash, vehicles, parts, setups, mastery, career perks, paints, departures,
settings, records and challenge progress in the active profile save automatically after banking a
run or making a menu change. Battery saves from the previous coastal release
retain their full progression and settings. First-draft cartridge battery saves
migrate to the expanded format, retaining their purchases and records. Version 4
adds installation history, smoke colors and presets to the same packed 500-byte
profile image. The opening stores three such profiles in the larger 8 KiB bank. Previous color-only
Apex, Signal and Midnight selections migrate to Fusion; existing Fusion choices
are retained. Choose Fusion, Pearl or Voltage in Settings. Delta
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
python3 drive/tests/playthrough.py --output drive/build/fusion-styles-final
python3 drive/tests/studio.py --output drive/build/fusion-styles-final
python3 drive/tests/opening.py --output drive/build/fusion-styles-final
python3 drive/tests/migration.py --output drive/build/fusion-styles-final
python3 drive/tests/studio_migration.py --output drive/build/fusion-styles-final
# Requires opening.py captures from pinned release dfca164, as documented in the script:
python3 drive/tests/previous_profiles.py --source drive/build/opening-validation --output drive/build/fusion-styles-final
python3 drive/tools/package.py --validation drive/build/fusion-styles-final
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
Buildings use separate wall and roof textures projected through the road's
camera. Architecture and local vegetation differ by region, with frontage,
set-back neighborhoods and distant scenery filling the sides of the view. Their vertical edges stay upright, and depth edges converge toward the
same horizon point as the highway. Houses occupy frontage plots, trees grow in
irregular clusters, and larger waterfront structures have wider spacing.
Trees and lamps attach at their roots and pole bases. Upright boat profiles
rise above their waterline, broadside to the road. Docks extend perpendicular
to the highway from the shoreline, with one or two connected sections.
Sun, moon, stars and local atmospheric effects distinguish the regions.
Location previews remain in the highlighted region while animating the road.
Overhead signs and roadside billboards remain removed.
Traffic follows safe gaps instead of driving through other cars.
Handling, rewards and progression remain subject to
playtesting. The earlier games remain in their own folders and branches.
