# Shoulder-support catalog

CSS 1.0.1 reads `catalog/hand-support.json` from its mod folder once at startup.
The shipped source is `packaging/catalog/hand-support.json`. Editing a binding
or calibration requires a game restart, not a DLL rebuild. CSS Shared Assets
1.0.1 provides the cooked rigs; the JSON belongs to the runtime download.

The catalog contains:

- `weapons`: unique IDs and exact weapon class paths.
- `bindings`: exact mesh/post-process pairs, original/replacement rig classes,
  finger/grip profile IDs and bones transferred by the rig node.
- `targets`: a profile ID, weapon ID and 19 measured local finger rotations.
- `grips`: a profile ID, weapon ID, rotation and translation from prop to hand.
- `carry`: pose ranges, gait interpolation anchors and blend durations.

Quaternions use `[x, y, z, w]`; translations and pose ranges use Unreal
centimetres. Durations are seconds. Existing measurements are preserved exactly
from the accepted R7 tables. The loader does not renormalize or retune them.
Different mesh bindings can share finger profiles and retain separate grips.
IDs are data; adding a compatible outfit or weapon does not require a new enum.
Each binding must have matching target and grip weapon sets. A weapon outside
those sets receives no shoulder carry correction.

Schema 1 uses the `shoulder-support-v1` cooked-rig interface. The 19-entry
`target_bones` order must match that interface. Rig property names, property
types, animation synchronization, the IK implementation and its bone contract
remain code or cooked assets. A different rig interface still requires an
implementation change; merely listing an incompatible rig is not sufficient.

The loader rejects duplicate JSON keys and identities, missing references,
invalid paths, wrong bone/array lengths, nonfinite values, nonunit quaternions,
invalid ranges and oversized inputs. Missing or invalid files disable this
feature and log the reason without preventing CSS from loading. There is no
compiled calibration fallback. The customization service reports
`hand_support.catalog_loaded` and `hand_support.catalog_error`.

`export_carry_targets.py` and `export_grip_references.py` under
`tools/authoring-probes/hand-transfer/` update the corresponding sections of an
existing catalog from audited measurement inputs. They no longer emit headers.
New bindings still require compatible cooked rigs and measured validation;
copying another body's calibration is not evidence of correct fit.

## Runtime and regression checks

The loader owns immutable strings and typed arrays for the core's lifetime.
Lookups borrow those strings with `string_view`; transparent map comparators
avoid temporary keys. Parsing, validation and allocations happen at startup,
not per frame. Missing entries return no match. No new animation hook, worker
thread write or change to the existing animation barrier is introduced.

The catalog has passed 28 host suites, 15 archive tests and AddressSanitizer /
UndefinedBehaviorSanitizer checks. The migration compares every value against
an independent dump of the accepted compiled tables. Five million new-catalog
lookup sets allocated nothing; median lookup time was 124 ns on this machine
(compiled tables: 27 ns). This measures binding/weapon/profile lookup, not
complete frame time or rendering performance. Evidence is under
`work/shoulder-weapon-hold/catalog-*`.

The review follows the allocation and lifetime guidance in the
[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
and Epic's guidance to account for game-thread and worker-thread animation work
in [UE 5.6 animation optimization](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-optimization-in-unreal-engine?application_version=5.6).
The existing UE 5.6.1 rig/input fixtures remain the engine-specific regression
baseline. These checks do not establish that every gameplay path is bug-free.
