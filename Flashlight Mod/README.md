# Flashlight Mod — Arma Reforger

Custom flashlight component for Arma Reforger with extra tactical features.

## Features

- **Death Drop** — flashlight has a configurable chance to drop when the owner dies
- **Death Timer** — light stays on for a configurable duration after the owner dies (good for finding bodies)
- **Strobe Patterns** — define any number of named strobe patterns (speed configurable per pattern) and cycle through them in-game via inspect action
- **Brightness Levels** — define any number of named brightness levels (multiplier configurable per level) and cycle through them in-game via inspect action
- **State Persistence** — strobe mode and brightness level persist if the flashlight is dropped or the character dies

## Files

| File | Description |
|------|-------------|
| `SCR_FlashlightComponent2.c` | Main component — attach to flashlight prefab instead of `SCR_FlashlightComponent` |
| `SCR_StrobeFlashlightAction.c` | Action script — add to `ActionsManagerComponent` on the prefab |
| `SCR_CycleFlashlightBrightnessAction.c` | Action script — add to `ActionsManagerComponent` on the prefab |

## Setup

1. Add `SCR_FlashlightCustomMade` component to your flashlight prefab (replaces base `SCR_FlashlightComponent`)
2. In the **Strobe Patterns** array, add entries with a name and interval (seconds)
3. In the **Brightness Levels** array, add entries with a name and multiplier (1.0 = default, 2.0 = double, 0.3 = dim)
4. Add `SCR_StrobeFlashlightAction` and `SCR_CycleFlashlightBrightnessAction` to the `ActionsManagerComponent`
5. Both actions appear when the player **inspects** the flashlight while the light is on

## Credit

Made by steffenbk. Free to use — credit appreciated.
