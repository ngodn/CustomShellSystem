# Sidearm Tarstone visibility

October 14, 2026. Investigation only; no runtime repair or new installation.
The October 13 discovery identified the equipped sidearm as `WP_NailShotgun_C`.
The October 14 live capture below now identifies its attached Burn effect,
but does not reproduce the reported visible residue.

## Live infused/stowed state, October 14

`live-effects-20261014.jsonl` contains three complete samples from the running
game. Naylshotte is on `Socket_NailShotgun_Stowed` with Burn/OnCrit tags.
Its actor, WeaponMesh and SM_Weapon are hidden. `SpawnedPermanent` is an active,
component-visible `PS_Nailshot_Burn` attached to SM_Weapon. All expected
effect fields resolve, and the player remains the same throughout.

`effect-owner-20261014.jsonl` verifies GetOwner and WasRecentlyRendered
signatures before calling them. The effect belongs to the hidden Naylshotte
actor, and WasRecentlyRendered(0.5) is false. An additional controlled switch
from Iris Out to the original idle leaves the actor/meshes hidden across five
samples; the effect remains not recently rendered. Iris Out and the entire
saved settings document are restored, with the same player. Evidence is
`idle-release-20261014.json` and its request log. No gameplay input was sent.

These observations do not reproduce the visible residue. In particular,
the effect's own bHiddenInGame=false is insufficient to diagnose a failure:
the owner is hidden. Local UE 5.6.1 PrimitiveSceneProxy.cpp:340 reads the
owner's hidden flag, and NiagaraComponent.cpp:344 uses IsShown in draw
relevance. Actor.cpp:5242 updates component visibility when actor hidden
state changes. This agrees with Epic's
[SetActorHiddenInGame contract](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/GameFramework/AActor/SetActorHiddenInGame?application_version=5.5).
The game fork and view-dependent rendering still require actual reproduction.
No automatic effect-hiding patch is justified by the active flag alone.

The game was off the active display during these checks, and the current
camera does not establish a clear view of the stowed effect. Do not report
the bug fixed or a visual pass. Still capture the visible failure after
aim/fire/stow or effect replacement; compare the owner and mesh flags then.

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
output or writing a request. The October 14 run above also passes live property
discovery and captures the infused/stowed state; visual failure reproduction
remains pending.
When the game is available, capture the reported infused/stowed state and
compare Default, hidden and in-use settings with equip/aim/fire/stow/swap.
Then cover effect replacement, form/save changes and restoration. A capture
completing by itself is not a visibility pass.
