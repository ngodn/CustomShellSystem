# Fresh shipping runtime installation

The exact local CSS 1.0.0 runtime ZIP passes installation into an empty CSS
directory, startup, default-state creation, asset discovery and Peony facial
playback. The original runtime and exact saved CSS settings are restored.

Archive: `dist/v1.0.0/MSII-CSS-v1.0.0.zip`.
SHA256: `c2c084b72b8ac4cacd6b355c93c328af60de014e654d32bb4a05c02be215c719`.
Evidence: `work/three-outfit-rig-physics/clean-runtime-install-20261015/`.

## Actual installation

The idle/input preflight passes in process 723674. Existing game saves are
copied and hash-verified for recovery, without writing them back. Native quit
finishes before the original CSS directory moves by rename to a holding path.
Only the verified runtime ZIP is extracted into the new CSS directory. It has
no state, cache or runtime directory before launch. Every installed payload hash
matches the archive manifest, including its shipping loader and core.

The existing accepted outfit/shared containers and CSSX remain installed.
CSSX supplies the external reflection channel; the tested CSS core is the
shipping binary, not a developer substitute. No asset containers are replaced.

Process 741340 starts with no applied appearance, empty selections and profiles,
and no maintenance error. All four expected asset providers load. Selecting
Peony succeeds, Iris Out activates the exact `SK_PeonyV2_Face` mesh, and its
performance time advances. CINE's model and take control are available. The
temporary performance stops before normal quit.

## Restoration

The original directory returns intact while the game is stopped. Its loader,
core selection, core DLL and full CSS state match the saved baseline. Process
743885 continues the existing save and restores Merrick, original customization
and idle selection. `result.json` and the independent `postcheck.json` pass.
No game save backup is restored; normal saving during quit remains allowed.

The first shipping-observer request used CSS's `engine` operation through CSSX,
which does not expose that operation. It failed before Continue. The recipe now
uses CSSX's documented player-world handle. During original-runtime recovery,
the native shader-compilation check initially reports work pending. The game
stays running; after it reports false, the established menu-creation and Continue
path succeeds. Neither observation is counted as a product fix. Request logs
retain the failures and successful follow-ups.

## Scope and cleanup

This closes fresh CSS runtime/default-state startup against the current accepted
asset providers. It does not claim a mounted legacy-provider upgrade, a fresh
game installation or completed CINE recording. Those are separate from the
successful runtime installation and facial playback observed here.

The disposable test installation and its 746,358,394 bytes of files were removed
after hashing and successful restoration. The archive, recipe, request logs,
clean-state/playback receipts and recovery copies remain. See the
[cleanup record](../development/cleanup-2026-10-15-clean-runtime.md).
