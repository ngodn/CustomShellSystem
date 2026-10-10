# Peony combined outfit and shared-assets trial

The private seven-outfit Eve trial, expanded shared pack and matching CSS core
are installed. Their installed hashes pass verification. This enables the
gameplay-pose comparison needed for the unresolved hip-accessory contact.
It is not release acceptance. The user has been asked to check idle/walking fit,
Iris Out and attack interruption; results are pending.

## Package contents and preservation

`peony-combined3` contains 363 Eve packages and 279 shared packages. It adds the
55 cooked Peony outfit packages and ten shared material/function packages to
the verified existing containers. Every original staged payload is checked
against its previous verification receipt. All non-header payloads, old and
new, survive the combined IoStore round trip unchanged.

The seventh Eve variant uses the existing Peony catalog candidate: 40 controls,
nine dye surfaces, five palettes and automatic boot-height adjustment. The
original six variant definitions and dye-resource bytes remain unchanged.
Peony's normal/facial meshes, body morphs, cloth buffers and shader verification
are covered by the [Windows cook receipt](2026-10-11-peony-windows-cook.md).

Shared metadata starts from the installed package, not its older staging copy.
This matters because the installed Commander White strand-rig and Genessa
world-cloth revisions are newer. Their facial and animation adapters stay
unchanged. Eve's adapters receive the new container identity, and its facial
rig gains one normal-to-facial Peony mapping. Existing animation entries and
all facial performance resource bytes remain unchanged.

## Verification

- Both rebuilt IoStore containers pass `retoc verify`.
- The real native catalog reader accepts the three outfits and one shared
  library with no rejected package. The extended ten-variant audit passes
  1,535 checks, covering 110 bundled animation choices, ten facial matches,
  600 CINE clip resolutions, revision rejection and existing adapter behavior.
- Independent cooked dependency inspection passes for 657 packages and 230
  actual object references, with zero unresolved imports or missing exports.
- Astral lifecycle/policy, animation library, facial library, customization
  controls and skeleton compatibility CTest targets all pass.

The first assembly stopped before conversion because the unpack destination's
parent did not exist. The second built valid containers and passed native
catalog/round-trip checks, but its reference-list audit incorrectly treated
historical animation aliases as loadable assets and invented `Package.Package`
references for generated Blueprint classes. The third run fixes the audit:
actual catalog references are checked as written, their packages are included,
and the native selector tests cover legacy aliases. No shipping asset was
changed to make that audit pass.

## Installation and rollback

Evidence and candidate:
`work/three-outfit-rig-physics/peony-combined3`, linked to
`/mnt/eins0fxE/CSSAuthoringWork/three-outfit-rig-physics/peony-combined3`.

Installed core:
`css_core-peony-6d751cbdab2cab4c.dll`, SHA-256
`6d751cbdab2cab4c9bfd9ea4420c3cbca1706fbf9c44a66a27f8b2e8299a3dd0`.
The prior core was `css_core-cw-hair-718f76c3b60a8315.dll`.

Rollback copies are in
`/run/media/eins0fx/theark/CSS/peony-before-combined3`.
They include both previous package trios, the previous core and its selector.
The installer checks that the game is closed, verifies backups and staged
copies, swaps package directories with rollback handling, then selects the
new core. It preserves profiles, settings, the loader, CINE and other outfits.
`deployment.json` records the installed paths and hashes; no input or camera
automation was sent to the game.

The loader contract's DLL checksum is stale. Installation initially stopped
without changing the game. The installed loader is identified by the earlier
`shared-cloth-native2/installed.json` receipt, and all six PE section hashes
match the current loader build. All recorded loader-source hashes also match.
`retained-loader.json` records this evidence. The existing loader and contract
file were kept unchanged; core selection preserves the existing selector ABI.

After both assembly processes had ended, the failed second attempt's staged
and round-trip files were compared byte-for-byte with the passing third one.
Removing only those duplicates removed 4,154,569,807 logical bytes. Its recipe,
metadata, logs and `cleanup.json` remain. Its package trios were retained
because their PAK bytes differed; this cleanup does not claim an equivalent
amount of physical free space on the reflink filesystem.

## Remaining work

The retained accessory fit reduces the mount gap, repairs root-weight stretch
and supports body morphs, but the captured editor walk still intersects the
arm. The underlying body also crosses in that pose. This trial is needed to
compare the actual game graph before another fit or motion change; it does not
accept that clipping or change shared gait motion. Final material/glow/opacity,
modular visibility, cloth, facial, doubles, lifecycle and performance checks
remain. Slipstream, the queued CW repairs and the wider release goal remain
active.
