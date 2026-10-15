# Accepted native feature review

Local source checkpoints, not a release:

- `86f4643`: boss sizing, native shades and confirmed one-time revival override.
- `eb38dba`: accepted boss assets integrated into the shared-pack recipe/catalog.

Review checked bounded revival-hook ownership, mesh-scale restoration, weapon
scale compensation, exact skeleton allowlists and native shade unlock handling.
The decoded native revival predicate checks `State.ReviveBlock`; the override
remains scoped to the confirmed request. This does not promise unchanged saved
form or health. No new runtime change or install was made during this review.

`cmake --build build/host -j4` succeeds and all 26 host CTest suites pass.
The test log is retained at
`work/three-outfit-rig-physics/native-features-review-20261015/host-tests.log`.
This review does not complete the outstanding gameplay or stable-release gates.
