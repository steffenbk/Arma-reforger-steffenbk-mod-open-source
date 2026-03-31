# No More Planking State

Keeps characters in their ragdoll pose while unconscious instead of blending into the stiff animated prone position.

- All characters (players and AI)
- Multiplayer and dedicated server compatible
- Vanilla revive animation works normally
- No prefab overrides -- script only

## Performance

- Runs only on entity owner (server for AI, owning client for players) -- other clients get ragdoll state via engine replication at zero extra cost
- Single timer call per unconscious character per second, no per-frame logic
- Zero overhead when no characters are unconscious
- Ragdoll bones settle within seconds so ongoing physics cost is near-zero even while active

## How it works

Mods `SCR_CharacterDamageManagerComponent.OnLifeStateChanged`. When a character becomes INCAPACITATED, starts a repeating `RefreshRagdoll` call to prevent the engine from timing out the ragdoll and blending to animation. On wake-up or death, calls `RefreshRagdoll(0.0)` to immediately release the ragdoll so vanilla transitions work normally.

## Compatibility

Script only, no asset overrides. Compatible with other mods including those that modify animation graphs.
