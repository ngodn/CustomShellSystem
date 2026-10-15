# Stable runtime review

Independent standards and specification reviews covered the runtime/test diff
from the goal's `dddaf703fc8df00f2b38e46e08fe01426b1e0694` baseline through
`2fcf221`. Both found a concrete issue. Both fixes are committed and built;
the candidate is not installed or live-verified yet.

## Standards

The developer-only reference-pose trial recreated its native animation instance
at timeout without checking whether movement or a montage had started during
the trial. That could clear a newly started native action. It violated the
goal's requirement to preserve graph/montage ownership on interruption.

Commit `92f49c3` rechecks the current pawn and mesh, custom playback and cleanup,
menu state, finite idle speed and native montage activity before recreation.
Temporary activity or a temporarily missing current player keeps cleanup
pending. Forced core unload refuses to bypass this condition.

An expired/replaced owner or reassigned mesh is handled separately. The trial
releases its reference flag only if it still matches its own write, then drops
its handles without recreating the retired instance. That is flag release,
not reconstruction of private state on the retired graph. Reflected-call
failures retain the trial for retry. Follow-up static review found no further
concrete issue. The path is limited to `CSS_INVENTORY_DEV`; no production-build
failure was established and no live reference-pose trial was run here.

## Specification

Profile preflight used the ordinary appearance resolver, which skips missing
mirror providers. While Stray with mirroring enabled, a profile containing a
living-shell outfit whose provider was absent could fall through to native or
the separate Stray choice. It then changed global settings and selections
without reporting the missing package. This violated the requirement to
preserve valid saved state when assets are unavailable.

Commit `ac62655` lets the profile resolver identify a missing mirror provider
before validation. Existing unavailable-package guidance now rejects the load
before any state change. Ordinary appearance reconciliation still skips missing
providers; installed incompatible mirror choices keep their existing behavior.

The new test failed before the fix with `Missing mirrored outfit was silently
replaced by a fallback`. It now covers remembered and inferred living-shell
slots, separate Stray choices present/absent, unchanged serialized state,
disabled mirroring, ordinary runtime fallback and provider reinstall. Follow-up
static review found no further blocking issue.

Initial findings: Standards 1 (developer cleanup), Spec 1 (mirrored preflight).
Both are fixed in the candidate; live validation remains separate.

## Checks and artifacts

Evidence: `build/stable-runtime-review-20261015/`.

- C++23 host build and all 26 CTest suites pass.
- The documented Python suite passes all 83 tests. Its initial two failures
  and one error exposed assumptions in the path-guard harness, not broken
  authoring paths. Commit `bec5bc7` accounts for paths normalized before the
  guard and excludes dynamic receipt comparisons from fixed-output checks.
  It exercises 25 actual fixed-path guards. No Slipstream or accepted authoring
  recipe was changed.
- Explicit namespace transaction tests pass 24 cases; hand ownership passes
  six; secondary bindings pass 14.
- Three current/archived state snapshots and their 12 stored profiles pass
  parsing, serialization and global restoration checks. Their input hashes
  remain unchanged.
- Both Windows development and shipping `css_core` builds pass. The shipping
  build retains the unrelated existing unused `row` warning in
  `extension_data.cpp:140`.
- `git diff --check` passes. No mounted outfit asset, game setting, save,
  gameplay action or core selector was changed.

Candidates and source/build hashes are retained in
`work/three-outfit-rig-physics/runtime-review-20261015/candidate.json`, with the
exact source patch and two DLLs:

| Build | SHA-256 |
| --- | --- |
| Development | `a5f4fef63056904f5821cb74aad8fd2ce3ea884ab970291a3d57781139b35d06` |
| Shipping | `952afa47d1fbfb10fcdc299c747dd6662249aba18c7879e4293f7299838e38a0` |

The installed core remains
`css_core-development-5f04a31900c8e323-nested-visibility.dll`, hash
`5f04a31900c8e323a27f6ed5650649595015b6dea6600f6682db582dc8fcd619`.
Next: guarded stationary activation of the completed development candidate and
normal live profile regression, retaining exact user-state restoration. These
checks do not replace the remaining full-goal lifecycle and release requirements.
