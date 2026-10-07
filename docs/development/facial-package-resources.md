# Packaged facial performance resources

Facial curves can be larger than the 256 KiB CSS.Package manifest limit.
Animation libraries may now store those curves in separate metadata entries.
The limit on the manifest is unchanged. Outfit packages and old inline facial
catalogs need no changes.

This format requires the CSS runtime change in `native/src/packages.cpp`.
Older runtimes do not resolve these external tracks. Distribute the new pack
with its matching CSS update; this private checkpoint does not assign release
versions or authorize installation.

## Format and bounds

An `animation_library` manifest may declare:

```json
"facial_resources": [
  {
    "rig": "css.commanderwhite.face",
    "file": "css.commanderwhite.face.face.json",
    "bytes": 1234,
    "sha256": "<SHA-256 of the resource bytes>"
  }
]
```

The resource sits beside `manifest.json` inside the metadata pak, under
`MortalShell2/Content/CSS/Packages/<package-id>/`. Its filename must be
`<rig-id>.face.json`; paths and alternative names are rejected. Its JSON is:

```json
{"schema":1,"rig":"css.commanderwhite.face","performances":[]}
```

The `performances` objects use the existing facial catalog format. The rig
must be declared exactly once in the manifest and must omit its inline
`performances` field. A rig cannot mix inline and external tracks, even with
an empty inline list. Other rigs may still use inline tracks.

- At most 16 resource descriptors, 16 MiB per resource and 64 MiB total facial
  resource bytes in one metadata pak.
- Manifest remains at most 256 KiB; overall metadata remains at most 260 MiB.
- Pak entry SHA-1, declared byte count and resource SHA-256 are checked before
  using the data. Schema, rig identity and descriptor fields are checked.
- Existing parser bounds still apply: 128 exposed morphs per rig, 64 facial
  performances per rig, 65,536 keys per performance and 262,144 keys per rig.
  Curves must reference a declared morph and a valid body clip in the library.

Resolution happens during catalog discovery, before the existing facial
parser runs. CSS and CINE receive the same resolved catalog. Playback adds no
resource reads or JSON parsing. The loader moves the parsed performance data
into the catalog rather than copying that tree again.

A missing, damaged or invalid resource rejects its owning library package
and records a diagnostic. It does not erase healthy outfit catalogs. Normal
saved-choice and bundled-animation fallback rules still apply. This is not
partial recovery of the damaged library's body entries.

## Authoring and verified private candidate

`tools/css_facial_resources.py` externalizes and resolves authored manifests,
checking an exact round-trip. `package_shared_library.py` accepts the explicit
`--external-facial-resources` flag and verifies the unpacked resource bytes.
Without that flag it retains inline packaging and rejects an oversized
manifest. `package_facial_resource_trial.py` migrates a verified private
candidate without cooking, altering source packages or installing anything.

`work/cine-animation-port/facial-resource-trial-v1` migrates `cw-jaw-trial-v1`:
63 animation entries, 98 unchanged cooked exports, six Eve facial tracks and
one Commander White facial track. Its manifest is 37,884 bytes instead of
244,391 bytes. Both `.utoc` and `.ucas` are hard links to the verified inputs;
only the metadata pak differs. Source and packaged manifest comparison, both
resource hashes and complete resolved-catalog equality pass.

Spin is tested as a large parser fixture using its real 657,779-byte prepared
performance. It is not added to the playable candidate: its new shapes still
need cooking and integration. No new candidate was installed.

## Verification

`tests/facial_resource_packages.py` builds real V8B metadata paks with clearly
synthetic IoStore companions. All 32 cases pass, with 70 native checks and
24 authoring rejection checks. They include old inline data, the actual Spin
curves, missing resources, malformed JSON, bad size/hash, path traversal,
duplicate rigs/descriptors, mixed storage, excessive byte totals, unchanged
manifest limits and invalid curves/body references. Rejected packages leave
an independently loaded outfit intact. Temporary fixtures are removed.

The real combined candidate passes 1,398 host catalog checks; pack-only
discovery passes 1,386. The original inline candidate still passes 1,386.
Existing facial compatibility and sampling suites pass 86 and 56 checks;
the CW jaw corpus passes 220. Evidence is retained in
`facial-resource-package-tests.json` and
`facial-resource-trial-v1/host-validation.json` under the animation workspace.
These checks validate data loading and compatibility, not live frame times,
appearance or complete animation acceptance.

The Windows `css_core` target builds successfully with the existing MSVC ABI
toolchain. Its build emitted one pre-existing unused-variable warning in
`extension_data.cpp`; no new compiler warning came from these changes.

Reproduce the resource fixtures with:

```sh
cmake --build build/tests --target css_facial_resource_tests -j2
python3 tests/facial_resource_packages.py \
  --repak build/repak/release/repak \
  --test-binary build/tests/css_facial_resource_tests \
  --performance work/cine-animation-port/cw-spin-curves-v2/performance.json
```
