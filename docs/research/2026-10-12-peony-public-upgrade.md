# Peony public-package upgrade checks

The public Peony delivery was previously checked against the installed migrated
CW/Genessa trials. This follow-up checks the actual published Unholy Genessa
v0.2.0 and Commander White v0.0.6-dev alongside Eve v1.5.0 and shared beta.12.
No game files or release ZIPs were changed.

## Inputs and isolation

`tools/diagnostics/audit_peony_release_upgrade.py` checks each archive's SHA-256
sidecar, then streams every archived trio member and compares it with its retained
container. The old containers come from the original namespace migration backup;
their bytes match the published ZIPs. The new trios match the final delivery ZIPs.
No full copies or extracted bulk data are needed.

Each case mounts only the base game, new Eve/shared, and its explicitly selected
legacy outfit trios. It excludes installed trials and the old separate shared
packs. All supplied custom packages are inventoried before checking their public
import hashes. The same package manifests also pass through CSS's native catalog
loader with exact expected outfit/library counts and seven Eve facial bindings.

The initial `peony-public-upgrade1` stops because the older default auditor DLL
does not support `--package-name-paths`. It produces no import result and is not
a failed asset test. The second run explicitly selects the previously verified
extended auditor at `cw-retirement-audit1/bin/AssetDependencyAudit.dll`, hash
`dd313c55ac6638816daea15a06d8fd7ed9f0aede2ec4b976682e2558929827b7`.
That auditor's prior regression verifies unchanged default-mode results and
that the opt-in adds only conservative name paths and its scope description.

## Results

Evidence: `work/three-outfit-rig-physics/peony-public-upgrade2`.

| Mounted outfit combination, with new shared pack | Packages checked | Missing hard imports | Native catalog |
| --- | ---: | ---: | --- |
| New Eve only | 718 | 0 | Pass |
| New Eve + released Genessa | 849 | 0 | Pass |
| New Eve + released CW | 915 | 0 | Pass |
| New Eve + both released outfits | 1,045 | 0 | Pass |

All four auditor processes exit 0, all four catalog scenarios pass, and the
protected input hashes remain unchanged. The sole overlapping package when both
older outfits are present is `/Game/CSS/Shared/SKEL_Base`; both index entries
have the same export chunk hash. New Eve/shared introduces no overlapping owner.
This index comparison is not an independent re-decode of the old skeleton.

The seven facial adapter matches belong to the seven new Eve variants. The
released older outfit revisions correctly do not match adapters authored for
the migrated CW/Genessa trials. A visible wardrobe entry does not provide the
new facial/playback support to an old package. The public instructions now state
this limitation instead of implying that updating shared assets upgrades outfits.

## Limits

Name maps also retain four Peony palette-prefix names, an old Genessa IdleSet
name, and four old CW material names without separately mounted packages.
These are conservative string candidates, not failed hard imports or proven
active soft references. Peony's actual four palettes have user acceptance.
Do not treat name-map leftovers alone as evidence of a broken material.

This check verifies dependency presence, public imports and catalog selection.
It does not execute soft references, render the old outfits, establish old
facial/playback compatibility, or complete the broader lifecycle matrix. The
earlier accepted gameplay result remains the evidence for Peony's appearance.
