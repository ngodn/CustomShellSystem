# Peony public release, October 12

The user deferred Slipstream and requested a public Nexus delivery. Release
files are in `dist/eve-v1.5.0`. Nothing was uploaded or installed during packaging.

- Eve v1.5.0: seven variants, including accepted Peony V2 and the six existing outfits.
- CSS v1.0.0-beta.12: the matching runtime, built from annotated local tag
  `v1.0.0-beta.12`, commit `6d5dd31` in a clean worktree.
- CSS Shared Assets v1.0.0-beta.12: matching animation, facial and double support.

Keep the three downloads separate. The outfit/shared ZIPs each contain exactly
one matching container trio. The runtime uses the existing release allowlist.
Readmes, BBCode changelogs, animation credits, SHA-256 sidecars and verification
receipts accompany the files. These are upload-ready files, not a published page.

The [public upgrade follow-up](../research/2026-10-12-peony-public-upgrade.md)
also passes cooked-import and native-catalog checks with released Genessa and
Commander White, separately and together. The accompanying instructions now
clarify that old outfit versions do not gain the newer trial facial/playback
adapters simply by updating shared assets. No release ZIP was changed by that
follow-up; its standalone README and evidence were updated.

## Evidence

The accepted inputs are `work/three-outfit-rig-physics/peony-v2-motion-package1`.
Both asset containers in each package are byte-identical to that candidate.
Decoded metadata differs only in the package and matching catalog version fields.
No Slipstream payloads or variant were added. Peony retains all four palettes,
removable pieces, flowing trim, body controls, facial playback and corrected motion.

`peony-release5` records 1,535 native catalog checks, three clean/missing/shared-only
dependency scenarios and 153 startup/cache/persistence checks. All pass. The
rebuilt host suite passes 25 tests; runtime archive tests pass 8 tests. Both final
asset ZIPs pass streamed payload-hash readback, and the runtime ZIP passes its
allowlist, PE-header, selector and payload-hash verifier.

The release core differs from the accepted core only in two COFF timestamp bytes
at offsets 128 and 129. Every PE section is identical. The accepted core hash is
`21f761aa35fd18619172db3e4b3b88318a42681bdcdac848412f45c3299acc37`;
the release core hash is
`0b69a50dae0092da86452480b521a01dbaa17b863f877f1d8622d9b284ae2d8f`.
This preserves the accepted core behavior. No new in-game beta.12 cold-start
check is claimed. The loader uses the new version label.

## Reproduction

Use the tagged `tools/css_release.py` for the runtime with the retained SDK
and runtime import library. Use tagged `tools/eve-fit/release_peony.py` for the
outfit/shared ZIPs, with the accepted candidate, fresh staging/output paths,
the existing tool build root and `--shared-metadata` pointing to:

`work/three-outfit-rig-physics/peony-release3/pack/CSS_SharedAssets_P/CSS_SharedAssets_P.pak`

This file is pinned by `packaging/shared-assets.json`. repak produced different
index bytes from identical metadata in the rehearsal and first tagged attempt;
the final recipe freezes this input and independently compares every extracted
file before accepting it. The first unpublished tag was replaced locally after
fixing that recipe; no remote tag or public release was changed.

## Remaining scope

Minor clipping is accepted for now. The complete earlier-outfit, both-form,
transition and performance matrix remains open, as does Vistani's sunlight-noise
report. Do not advertise those as fixed or fully tested. Slipstream sources and
evidence remain for later. The larger rig goal is still active.
