# Sidearm Tarstone visibility

October 14, 2026. Investigation only; no runtime repair or new installation.
The game is closed. The October 13 discovery identified the equipped sidearm
as `WP_NailShotgun_C`, but did not capture the lingering effect.

## Shipped asset evidence

Seven targeted game packages were decoded with the existing AssetReadback
tool and the installed CL93241 mappings. Evidence, package lists, extraction
logs and hashes are under
`work/three-outfit-rig-physics/sidearm-visibility1/weapon-effects`.

- `BPC_WeaponEffects.SetAttachComponent` obtains the weapon's static mesh
  through `BPI_WeaponBase.GetWeaponStaticMesh`, falling back to the owner's
  `WeaponMesh`. This becomes `AttachComponent`.
- `LoadPermanent` destroys the previous `SpawnedPermanent`, resolves the
  data object's `Permanent` system, spawns it on `AttachComponent`, and
  saves the returned Niagara component as `SpawnedPermanent`. This call uses
  no component pooling. The separate `LoadPermanentProjectile` path creates
  effects on visual projectile components.
- `WP_WeaponBase`'s generated event graph calls `LoadAllData` after adding,
  appending or removing weapon effect tags. `LoadAllData` calls both permanent
  effect loaders. Thus the effect component can change while the weapon
  remains equipped.
- `DT_WeaponVFXData` maps `Weapon.NailShotgun` to `VFXData_NailShotgun`.
  Its `Weapon.Effect.Burn` entry names `PS_Nailshot_Burn` as the permanent
  weapon effect. `PS_Nailshot_Projectile_Burn`, the muzzle and buff-completion
  effect are separate assets. This identifies a candidate for the orange
  weapon-shaped glow in the user's screenshot, not the actual live component.
- Trebuchaxe uses the same effect component infrastructure, with Lightning
  and Phantom permanent effects and separate projectile/trail systems.
  Its reported hovering axe still needs independent identification.

Epic documents [SpawnSystemAttached](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/Niagara/UNiagaraFunctionLibrary/SpawnSystemAttached)
as creating an attached Niagara component. The local UE 5.6.1
`NiagaraFunctionLibrary.cpp` additionally shows the non-pooled path creates
it with the attachment component's actor as owner. This engine source is a
reference, not proof that the game's fork has identical behavior.

## CSS observations and unresolved cause

`native/src/misc_visibility.inl` enumerates owned mesh components when hiding
a weapon, disables propagation, and also calls `SetActorHiddenInGame`.
The cached-item evaluation reasserts hiding only when the tracked root mesh
is not already hidden. It does not inspect the permanent effect or recheck
the actor's hidden flag in that condition.

This is a gap worth measuring, not enough evidence to choose a repair.
In particular, the actor's hidden flag should also matter for an owned
effect. A claim that excluding Niagara from the mesh list alone explains
the bug would omit that existing actor-level hide.

The next capture distinguishes:

1. A hidden mesh with a visible actor and active permanent effect, which
   would support the root-only reassertion problem.
2. A hidden actor whose permanent effect still renders, which requires
   checking component ownership and the effect's actual render behavior.
3. A visible effect outside `SpawnedPermanent`, such as a projectile or
   trail, which requires following that object's lifetime separately.

Do not enable blanket child propagation, destroy the infusion, or disable
all effects on the pawn. Earlier propagation exposed collision shapes.
Keep gameplay projectiles, impact effects and buff state intact.

## Prepared read-only capture

```sh
python3 -B tools/diagnostics/capture_sidearm_visibility.py \
  --output work/three-outfit-rig-physics/sidearm-visibility1/live-effects.jsonl \
  --seconds 30
```

The script resolves the current equipped Sidearm slot on every sample,
checks available reflected properties, records actor/root/effect flags and
attachment identities, and logs each request before sending it. It performs
only player discovery and property reads, with no gameplay input, setters,
function calls, settings changes or global object scans. It stops on a
process/player change or failed read. Samples are sequential, not atomic.

Help and the closed-game guard pass; the latter exits 2 before creating the
output or writing a request. Live property and visual checks remain pending.
When the game is available, capture the reported infused/stowed state and
compare Default, hidden and in-use settings with equip/aim/fire/stow/swap.
Then cover effect replacement, form/save changes and restoration. A capture
completing by itself is not a visibility pass.
