# Unholy Genessa live facial track checks

Both Unholy Genessa variants pass numeric facial playback and explicit-stop
restoration checks for Iris Out and Come Closer in the current game session.
This advances the facial regression gate beyond loaded-mesh/adapter evidence.
It does not replace the required close-up visual and gameplay checks.

| Variant | Clip | Paused poses | Facial weights compared | Stop restoration |
| --- | --- | ---: | ---: | --- |
| Eve Skin | Come Closer | 3 | 345 | Exact baseline |
| Eve Skin | Iris Out | 3 | 345 | Exact baseline |
| Genessa Skin | Come Closer | 3 | 345 | Exact baseline |
| Genessa Skin | Iris Out | 3 | 345 | Exact baseline |

Across all twelve poses, the largest weight error is `2.384185793236071e-08`.
The largest difference between the performance clock and adapter body time is
`7.629394538355427e-07` seconds. Both are below the diagnostic's `2e-5`
tolerance. Each case also resumes briefly and verifies the clock advances.
All 115 facial controls return exactly to their pre-performance weights after
the owned stop finishes.

## Method and ownership

Recipe: `tools/diagnostics/check_genessa_live_face_tracks.py`, Python 3.14.7.
Evidence: `work/three-outfit-rig-physics/genessa-live-face-20261014-2`.
The directory retains the executed recipe, request/reply log, complete measured
and expected values, source hashes, state snapshots, CINE models, result,
independent receipt verification and checksums.

The test uses the existing CSS performance service that CINE calls. It requires
CINE to report that its take has finished and no performance to be active.
It temporarily chooses the body-only `eve` idle to prevent the remembered
facial idle from competing with the test. For each variant and clip it:

1. Reads baseline weights on the player's actual mesh component.
2. Starts a performance, retains its returned ownership token and pauses after
   blending in. Only that token may be stopped by the diagnostic.
3. Seeks to zero, the most expressive authored instant (largest sum of absolute
   curve values), and 80% of the clip duration. It compares all 115 live morph
   weights with linear interpolation of the installed candidate's facial track.
   Controls absent from a clip must be zero at full performance weight.
4. Compares adapter body time with the same performance clock, resumes briefly,
   then explicitly stops and checks exact baseline restoration.

The face meshes resolve to `SK_EveW3_Face` and `SK_GenessaW3_Face` beneath
`/Game/CSS/eins0fx/UnholyGenessa/Faces`. All four cases use the expected
`Animations/Adapters/ABP_Genessa` class. An independent receipt check verifies
these identities against the shared manifest and the installed UG TOC hash and
bulk size. The current UG trio hashes match the previously retained package.

Peony, its customization and the original idle override are restored. Full
saved CSS JSON and the complete CINE model compare equal before and after.
PID 1944353 is unchanged. No movement, camera, CINE take, save-game, form,
package-installation or reload commands were issued.

The first attempt, `genessa-live-face-20261014`, rejected the fixture's assumed
`original` idle before testing. That sentinel is not in the service's returned
idle list. Its restoration and saved-state checks passed. The successful run
uses the advertised `eve` body-only option and validates it on both variants.
This was a diagnostic precondition error, not a game playback failure.

## Remaining scope

This checks the shared service while CINE is between takes, not an actual CINE
directed take. Paused samples and a short advancing interval do not establish
continuous lip/body synchronization, eye/mouth appearance, neutral geometry,
clip-end/loop behavior, movement/attack/aim interruption, both Genessa forms,
doubles or full lifecycle/performance coverage. Those gates remain open.
No rendered facial result is inferred solely from correct morph weights.

Repeat with a new output directory while CINE is between takes:

```sh
python3 tools/diagnostics/check_genessa_live_face_tracks.py \
  --output work/three-outfit-rig-physics/genessa-live-face-recheck
```

Only small diagnostic records were created. No cook, copy of the authoring
project, cleanup deletion or new package was needed. Free space during this
check was 92G on root and 67G on theark (`df -h`).
