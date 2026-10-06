# Shared animation outfit coverage

October 6, 2026. This records inspected assets, not release acceptance.

Support includes the original eins0fx-authored CSS collection, excluding
port-only mods, and compatible default-human-skeleton appearances from other
authors. Neither an author name nor a CSS manifest should be a requirement for
the eventual native-human playback path. The initial nine meshes are not the
whole collection.

## Additional collection inventory

Metadata was read from each actual PAK. TOC hashes and bulk sizes were compared
with its manifest. These checks do not claim complete bulk integrity,
editor/cooked graph equivalence or playback acceptance.

| Project | Package evidence | Animation work remaining |
| --- | --- | --- |
| GeneXXa / Nunexxa | Two v1.0.0 meshes; shared base reference and nine virtual-bone definitions match Eve exactly | New fixed-physics adapter passes 20 moving clips on both meshes; cooked comparison, runtime binding and live acceptance pending |
| Seductress | Six packaged variants: Midas, Argent, Noir, Violette, Regalia, Flora | Older 379-bone reference differs on 72 common bones; inspect retarget behavior and preserve BodyHairV42 graph |
| Seductress v2.0.6 | Manifest lists 18 variants, but local build TOC hash and bulk size differ from that manifest | Locate a consistent original package before using it as a fixture; do not alter this build to make the audit pass |
| Seductress Next-Gen Showcase | One packaged variant using the native human skeleton; mesh has no post-process graph | Native-human sequence and playback path needed |
| Protocol Echelon Proxima | Authoring project exists; design goal remains paused | Preserve paused project; do not treat it as a released animation fixture |
| KDA Takedown / Marci | Reference material only in the inspected project roots | No authored runtime package found in those roots; references are not playable variants |

Records include Seductress's `docs/handover-2026-09-21.md`, GeneXXa's
`claude-work/docs/plan.md`, and package manifests. The Showcase README has an
outdated engine claim; package and decoded-asset evidence take precedence.

Readbacks: `work/cine-animation-port/expanded-outfit-inventory/`. The inventory
records exact package paths, IDs, versions, mesh paths and TOC hashes. Each
eligible package was mounted separately with game containers, preventing one
outfit's shared assets from masking another's. Original sources, packages and
installed files were not modified.

## Native human path

`work/cine-animation-port/native-human-compatibility/assets.json` independently
decodes the current game skeleton and four representative native meshes.
The skeleton has 1,199 raw bones and nine virtual bones. It shares 189 named
bones with the inspected Eve skeleton; common parents match, but 87 local
reference transforms differ. This requires retarget validation, not just adding
the skeleton path to `compatible_animation_skeletons`.

ThornBoi, Gragu, Harros and Sester Genessa each bind a different native
post-process graph. Replacing those with an Eve graph would not preserve their
existing processing. Native support needs upstream playback or verified
adapters that retain that processing.

Current runtime restrictions still needing work:

- `PerformancePlayer` resolves adapters through exact outfit container revisions
  and requires existing CSS controls when transferring graph state.
- `Core::customize_api` and pending playback require an active CSS appearance
  and selection. Native/non-CSS appearances need an equivalent transient owner
  and restoration path.
- Animation compatibility recognizes exact skeletons and the audited
  base-to-wing mapping. Native human and older references need verified
  sequence alternatives or remapping.

These restrictions remain active until their replacement is tested. The audit
does not label unsupported cases as working or relax their checks.

## GeneXXa authoring extension

`CSSPerformanceLibrary` now inserts explicit-time playback at the linked input
of the audited GeneXXa graph, which has fixed physics nodes and no CSS idle
controls. Existing idle graphs still insert after the idle branch. Incomplete
idle contracts and ambiguous/missing inputs are rejected.

The C++20 helper builds with UE 5.6.1. Creation and fresh-process reload preserve
all 32 original GeneXXa nodes and their pin defaults, keep the original skeleton,
and start inactive. The five existing adapters also pass structural comparison.
`stage_genexxa_performance.py` provides create, verify and a 20-clip/two-mesh
evaluated-pose check. Runtime integration is not yet installed.

That evaluation completed: 40 clip/mesh pairs, 360 cases and 29,278 full-motion
poses at 60 Hz. Body and finger position differences are zero; all comparison
cases pass. `genexxa-evaluate.json` contains the measurements and protected
asset hashes. This validates preview-world evaluation, not gameplay cloth,
native cancellation or performance cost.

Facial capability remains separate. Missing expression controls must not
prevent compatible body animation playback.
