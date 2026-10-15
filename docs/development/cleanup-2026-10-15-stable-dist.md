# October 15 stable delivery staging cleanup

Written before removal, after the replacement Eve ZIP passed the package
verifier, archive integrity check and extracted-payload hash comparison.

- `work/three-outfit-rig-physics/stable-dist-20261015/eve-package`: disposable copies of the accepted cooked trio and regenerated versioned metadata pak; the verified ZIP, original accepted sources, checksums and recipe remain.
- `work/three-outfit-rig-physics/stable-dist-20261015/eve-metadata`: disposable copy of the original metadata tree with only two version fields changed; retain `eve-manifest-v1.5.0.json` and the exact recipe.

The withdrawn October 12 release rehearsal was moved intact to
`dist/withdrawn/eve-v1.5.0-beta12-rehearsal`. It was not deleted. No game,
accepted authoring input or published release was removed.

## Shared metadata correction

- `build/stable-asset-delivery/shared`: generated staging for the first stable shared candidate, whose nested animation-library version still said 0.1.0-dev. Rebuild from the retained accepted source with the corrected recipe.
- `dist/v1.0.0/CSS_SharedAssets_P-v1.0.0.zip` and its checksum: superseded local candidate, never published or installed. Regenerate with matching package and animation-library versions. Retain the first verification receipt under `stable-dist-20261015/shared-before-version-correction.json`.

## Completed outfit/shared staging

After all four final candidate packages pass the combined catalog check:

- `build/stable-asset-delivery/{genessa,commanderwhite,shared}/<container>`: duplicate generated cooked trios, retained in verified delivery ZIPs and accepted authoring sources.
- `build/stable-asset-delivery/{genessa,commanderwhite,shared}/{metadata,readback-metadata}`: regenerated metadata trees; before/after manifests and verification receipts remain beside the exact recipe.
- `build/stable-asset-delivery/catalog-fixture`: disposable package links and one extracted Eve metadata pak used for the combined catalog check.

Free space before this cleanup: root 94 GiB, theark 49 GiB (`df -h`).

Cleanup completed. `df -h` afterward: root 94 GiB, theark 49 GiB free.
The rounded free-space figures are unchanged because staging copies used reflinks.
