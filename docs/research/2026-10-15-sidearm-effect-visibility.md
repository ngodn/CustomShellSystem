# Naylshotte Burn visibility controls

The installed actor-visibility repair now has a controlled visual reproduction
of the lingering Burn symptom. With both Naylshotte meshes hidden and its owner
actor visible, the attached orange Burn effect remains visible. Returning to
**Only when in use** hides the owner and removes that effect from the image,
while preserving the permanent infusion. The [later gameplay check](2026-10-15-sidearm-aim.md) also verifies actual
draw/fire/stow. The original owner-drift trigger remains unproven.

## Inputs and method

Stage: `work/three-outfit-rig-physics/sidearm-current-20261015/`.
Recipe: `tools/diagnostics/check_sidearm_effect_visibility.py`.
Core: `css_core-development-d7cb0635d112f5fa-item-visibility.dll`, SHA-256
`d7cb0635d112f5fae73f67bc261c81d07df07eb0fef8797d786cc5d55191072f`.
Production source is committed in `5b74114`.

The current player is Peony in Faithful form, seated between CINE takes, with
Naylshotte stowed and the sidearm rule set to Only when in use. A passive capture
identifies `PS_Nailshot_Burn`, `Weapon.Effect.Burn` and `Weapon.Elemental.OnCrit`.
The Niagara component belongs to the weapon actor and is attached to `SM_Weapon`.
It is active, visible and not individually game-hidden. The actor and its two
mesh components are hidden.

The fixture uses normal MISC commands for Default and Only when in use. Under
Default, it also hides only the two weapon meshes with child propagation off.
That reproduces the actor/mesh mismatch found by the
[earlier form regression](2026-10-15-item-visibility-drift.md). It does not run
an old core or claim a natural gameplay reproduction. No effect, infusion,
loadout, ammo, asset or damage values are written.

Every phase samples `WasRecentlyRendered` three times with a 0.2-second
tolerance after a 0.7-second settling delay, then takes a Steam screenshot.
Signatures are resolved before calls. This function reports recent rendering,
not pixel visibility or a GPU duration, per
[Epic's API documentation](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UPrimitiveComponent/WasRecentlyRendered).
The screenshot observations below are separate evidence.

## Results

Both the original orientation and a temporary rear orientation pass all six
phases, giving 36 rendering observations:

| Phase | Owner hidden | Both meshes hidden | Burn recently rendered |
| --- | --- | --- | --- |
| Initial Only when in use | Yes | Yes | No, all samples |
| Default | No | No | Yes, all samples |
| Mesh-only control | No | Yes | Yes, all samples |
| Only when in use | Yes | Yes | No, all samples |
| Default repeated | No | No | Yes, all samples |
| Only when in use repeated | Yes | Yes | No, all samples |

The initial front view shows weapon visibility changing but obscures the Burn
effect behind the character. `rear-view.py` rotates only the player actor by
180 degrees in place, repeats the fixture, then restores the original rotation.
It does not move the camera or change CINE settings. The lower edge of the rear
screenshots contains the weapon area: `mesh-only-hidden.jpg` shows the orange
effect despite the hidden meshes, while `owner-hidden.jpg` removes it. Framing
clips part of the area, so these are diagnostic images, not release media.
Idle animation phases differ; this is not a matched-pose or lighting comparison.

Both fixtures preserve the same Niagara object, Burn asset, active/visibility
flags and weapon-effect tags. Default restores rendering after hiding, so the
test does not succeed by destroying or deactivating the effect. Full saved CSS
state, original weapon flags, appearance, CINE model, position, rotation, view
target and control rotation restore. PID 1944353 remains unchanged. All three
processes (front fixture, rear wrapper, rear fixture) exit zero.

## Scope and next check

This verifies that owner visibility controls the current attached Burn effect
and that the installed repair addresses the reproduced actor/mesh mismatch.
It strengthens the connection to the reported Tarstone symptom. It does not
prove that every occurrence of that report has this cause.

Actual aim/fire/stow now passes in the follow-up above. Next test effect replacement, including Barrage and
Emberseed behavior, while preserving infusion. The current test does not validate
projectiles, damage, ammo use, ability activation or a newly spawned effect.
No additional production change, package install or public release was made.
