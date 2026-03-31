# Spray Tagging Can - Modder's User Guide

A guide for anyone who wants to customize or extend the spray can mod.

---

## File Overview

```
Scripts/Game/
  SCR_SpraySizeManagerComponent.c   — Main config: colors, sizes, modes, actions
  SCR_SprayProjectile.c             — Decal spawning logic (raycasting, placement)
  MagazineWellSprayCan.c            — Magazine well type for the weapon
  SCR_SprayRaycastComponent.c       — Legacy component (not used by the prefab)
  SCR_SprayPaintDecalEffect.c       — Legacy effect component (not used by the prefab)

Prefabs/Weapons/Handguns/M9/
  Sprayer_new_test.et               — The weapon prefab. All in-game values live here.

Prefabs/Weapons/Magazines/
  Spray_mag.et                      — Magazine prefab (ammo count is set by script)

Assets/Decals/Paint/Vibrant/Data/
  Paint_White.emat ... Paint_Black.emat    — 8 free paint color materials

Assets/Decals/Paint/Stencil/Data/
  Stencil_X_White.emat ... etc             — Stencil color materials (shape + color)

Assets/Decal opaticy maps/
  Paint_Sam.emat, Paint_X.emat, etc        — Additional stencil materials
```

---

## The Weapon Prefab (Sprayer_new_test.et)

This is where you change most gameplay values. Open it in Workbench and look for the `SCR_SpraySizeManagerComponent`.

### Changing Ammo Count

Find `m_iAmmoCount` in the component. Default is 9000. Set it to whatever you want.

---

## SCR_SpraySizeManagerComponent.c

This is the main configuration script. It controls everything the player sees in the inspect menu and pushes settings to the projectile.

### Size Presets (`SCR_SizePreset`)

Controls the size options available in the inspect menu.

```
m_sName       — Label shown in the menu (e.g. "Small")
m_fDecalSize  — Diameter of the decal in meters (0.1 – 2.0)
m_fMinSpacing — Minimum distance from other decals before a new one is allowed (meters)
```

Add or remove entries in `m_aPresets` in the prefab to add more size options.

### Spray Modes (`SCR_SprayModePreset`)

A mode groups together a set of colors and controls behavior.

```
m_sName           — Label shown in menu (e.g. "Free Paint", "Stencil")
m_bRandomRotation — If true, each decal is rotated randomly. Set to false for symbols/stencils.
m_bAutoCycle      — If true, automatically steps to the next color after each spray.
m_aColors         — List of color entries available in this mode.
```

The current mod has two modes: **Free Paint** (random rotation on) and **Stencil** (random rotation off).

### Color Entries (`SCR_SprayColorEntry`)

Each color entry inside a mode points to a `.emat` material file.

```
m_sName     — Label shown in the menu (e.g. "Red")
m_sMaterial — Path to the .emat decal material file
```

**To add a new color:** create a new `.emat` file (copy an existing one, change the `Color` property), then add a new `SCR_SprayColorEntry` in the prefab pointing to it.

### Opacity Presets (`SCR_SprayOpacityPreset`)

```
m_sName  — Label (e.g. "Heavy")
m_fAlpha — Opacity value from 0.0 (invisible) to 1.0 (full)
```

### Brightness Presets (`SCR_SprayBrightnessPreset`)

```
m_sName       — Label (e.g. "Dim")
m_fBrightness — Brightness multiplier from 0.1 (very dark) to 1.0 (full bright)
```

The default is Dim (0.35) so decals look natural rather than glowing.

### Laser Dot

```
m_bShowLaserDot — Toggle the red aim dot on/off
m_fLaserRange   — How far (meters) the dot will project
```

---

## SCR_SprayProjectile.c

This script runs each time the weapon fires. It does the raycast, finds the surface, and spawns the decal. You generally do not need to edit this unless you want to change how decals are placed.

### Key values on the component (set in the projectile prefab or magazine)

```
m_fMaxRange       — Maximum spray distance in meters (default 20)
m_fDecalSize      — Fallback decal size if no manager override is active
m_fMinSpacing     — Fallback spacing if no manager override is active
m_fDecalLifetime  — How long decals stay in the world in seconds (default 1000)
m_bDebug          — Enable to print placement info to the log
```

### How it works

1. On spawn, fires a raycast forward from the projectile position.
2. If the normal is reversed (can happen on glass), it flips it.
3. Checks spacing — skips placement if too close to an existing decal, unless the new decal is larger.
4. Walks up to the root parent entity so decals attach to the whole object, not just a sub-piece.
5. Picks a material from the manager override, or falls back to the weighted list on the component.
6. Calls `World.CreateDecal()` with the ARGB color built from opacity and brightness settings.
7. Removes any smaller decals that fall within the new decal's footprint.
8. Tracks position + expiry so spacing checks stay accurate over time.

### Decal color

The color passed to `CreateDecal` is only used for opacity and brightness — it is always uniform grey/white. The actual hue (red, blue, etc.) is baked into the `.emat` material file. This is intentional — using the color parameter for hue tinting was unreliable across different surfaces.

---

## Adding a New Color

1. Copy `Assets/Decals/Paint/Vibrant/Data/Paint_White.emat`
2. Rename it (e.g. `Paint_Pink.emat`)
3. Open it and change the `Color` property to your RGB values (range 0–1 per channel)
4. Open the weapon prefab in Workbench
5. Find `SCR_SpraySizeManagerComponent` > `m_aModes` > Free Paint > `m_aColors`
6. Add a new `SCR_SprayColorEntry` with a name and point it to your new `.emat`

---

## Adding a New Stencil Shape

1. Create or find a mask texture (black and white, white = visible area)
2. Create an `.emat` file. Copy an existing stencil `.emat` and change `OpacityMap` to your texture.
3. Make multiple copies for each color you want (e.g. `Stencil_Arrow_Red.emat`, `Stencil_Arrow_Blue.emat`)
4. In the prefab, go to the Stencil mode's `m_aColors` and add entries for each color variant.

---

## Adding a New Mode

1. In the prefab, add a new `SCR_SprayModePreset` to `m_aModes`.
2. Set `m_sName`, `m_bRandomRotation`, `m_bAutoCycle` as needed.
3. Add `SCR_SprayColorEntry` items to its `m_aColors`.
4. The Mode action in the inspect menu will automatically include it — no script changes needed.

---

## The Inspect Menu Actions

These are registered in the prefab under `ActionsManagerComponent` > `additionalActions`. Each action class lives at the bottom of `SCR_SpraySizeManagerComponent.c`.

| Action Class | What it does |
|---|---|
| `SCR_SprayAdjustSizeAction` | Cycles through size presets |
| `SCR_SprayCycleColorAction` | Cycles through colors in the current mode |
| `SCR_SprayCycleOpacityAction` | Cycles through opacity presets |
| `SCR_SprayCycleBrightnessAction` | Cycles through brightness presets |
| `SCR_SprayCycleModeAction` | Cycles between Free Paint / Stencil / etc |

Actions only appear when the player is inspecting the weapon (`ctrl.GetInspectEntity() == GetOwner()`). To add a new action, create a class that extends `ScriptedUserAction`, add `CanBeShownScript`, `CanBePerformedScript`, `PerformAction`, and `GetActionNameScript`, then register it in the prefab's `additionalActions` with `ParentContextList "SpraySize"`.
