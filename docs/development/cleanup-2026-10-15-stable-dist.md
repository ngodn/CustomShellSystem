# October 15 stable delivery staging cleanup

Written before removal, after the replacement Eve ZIP passed the package
verifier, archive integrity check and extracted-payload hash comparison.

- `work/three-outfit-rig-physics/stable-dist-20261015/eve-package`: disposable copies of the accepted cooked trio and regenerated versioned metadata pak; the verified ZIP, original accepted sources, checksums and recipe remain.
- `work/three-outfit-rig-physics/stable-dist-20261015/eve-metadata`: disposable copy of the original metadata tree with only two version fields changed; retain `eve-manifest-v1.5.0.json` and the exact recipe.

The withdrawn October 12 release rehearsal was moved intact to
`dist/withdrawn/eve-v1.5.0-beta12-rehearsal`. It was not deleted. No game,
accepted authoring input or published release was removed.
