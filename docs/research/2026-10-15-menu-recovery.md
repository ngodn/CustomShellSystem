# Outfit preview and world recovery

All ten release appearances pass the CSS-menu recovery check in Stray form.
Their preview mesh, post-process class, body settings, six sampled body morphs
and material-section visibility match the world character. Enabled measured
secondary rigs advance in the visible CSS preview and after returning to the
world. Full saved state and player context restore.

No production code, accepted outfit asset or solver setting was changed. The
test did not request game-save or progression writes. Installed core:
`a5f4fef63056904f5821cb74aad8fd2ce3ea884ab970291a3d57781139b35d06`.
Game process 1944353 remained running throughout.

## Coverage

| Appearances | Body fields compared | Morphs compared | Updating rigs in each sampled phase |
| --- | ---: | ---: | --- |
| Black Pearl, Planet Diving Suit, Skin Suit, Bikini, Knitwear, Alice, Peony | 20 each | 6 each | Hair and body |
| Unholy Genessa, Eve and Genessa variants | 20 each | 6 each | Body; hair remains disabled in the saved setup |
| Commander White | 26 | 6 | Hair, body and belt |

Each case temporarily wears the existing saved appearance, opens the game's
menu, selects the CSS tab and verifies that CSS reports it active. Two preview
samples prove advancing update counters without changing the instance. After
closing the menu, two world samples verify advancing counters and restored
settings. All sampled numeric state is finite. No reset-counter increase occurs
between either pair of timed samples. This does not measure the transition's
entire reset history or prove continuous-motion stability.

The matrix restores outfit memories, original Peony, saved idle 702 and its
actual Seated Recline binding, CINE, player/view, Health 72, Resolve 100 and
released input. A separate final read verifies 15 conditions against the
original pre-activation snapshot, including nested weapon flags and Trebuchaxe's
hidden preview axe. Menus, developer trials and doubles are inactive.

## Retained fixture corrections

The first matrix opened CSSX, which selects its own tab. The preview's hair/body
counters remained at their initial values there. The test stopped and restored
all state. It now selects CSS through the existing developer navigation command
and verifies the active-page state before measuring. No ticking change was made
to the game or CSS.

The second run passes the first three outfits, then stops on Bikini because it
compares allocated visibility arrays literally. The world has 24 false entries;
the preview has an empty array. Exact UE 5.6.1
`Engine/Source/Runtime/Engine/Private/Components/SkinnedMeshComponent.cpp`, lines
3812 through 3825, treats missing entries as visible in `IsMaterialSectionShown`.
The corrected test compares the hidden (LOD, material) pairs. Epic documents the
corresponding [skinned mesh visibility API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USkinnedMeshComponent).
The source hash and finding are retained in `visibility-semantics.json`.

Both failed runs retain their errors and passing restoration receipts. The
remaining seven appearances were tested after correcting the mask comparison;
the already completed three were not repeated. The combined verifier requires
exactly ten unique appearances, checks all restored state snapshots, recomputes
settings and counter comparisons, and matches sampled values against 3,142
logged reflected responses. It accepts only the explicitly identified initial
mask-fixture failure, not arbitrary failed runs.

## Evidence and limits

Evidence root: `work/three-outfit-rig-physics/menu-recovery-20261015/`.

- `preflight.json`: Peony preview owner inspection and seven restoration checks.
- `matrix/`: inactive CSSX-preview counter failure and complete restoration.
- `css-tab-matrix/`: three passing cases and the Bikini mask-fixture failure.
- `remaining/`: seven passing cases and complete restoration.
- `verified.json`: ten-case coverage, counter deltas, raw-response checks and hashes.
- `final-live-restoration.json`: independent final state, visibility and live-idle checks.
- `verify-final-state.py`: retained read-only final verification recipe.

Reproduction uses Python 3.14 and the existing CSSX and CSS developer commands:

```sh
PYTHONPATH=tools:tools/diagnostics python3 tools/diagnostics/check_outfit_menu_recovery.py \
  --output work/three-outfit-rig-physics/menu-recovery-new \
  --core-sha256 a5f4fef63056904f5821cb74aad8fd2ce3ea884ab970291a3d57781139b35d06
```

This closes sampled CSS-preview body/visibility parity and primary-rig update
recovery for the ten appearances in the current Stray save. It does not establish
continuous visual/contact quality, lighting, every attached cloth solver,
forced LOD transitions, Faithful coverage or wider save/world/death lifecycle.
The user's accepted Genessa preview-clipping limitation remains accepted.
