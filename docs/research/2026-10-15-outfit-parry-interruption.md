# Parry interruption across the release outfits

Mapped Q parries interrupt Iris Out and Come Closer across all ten release
appearances in Stray form. All twenty cases pass. The independent verifier
finds forty positive native `GA_Parry_Action_Single` observations in the raw
request logs. It excludes the always-active parry handler, so an available
handler alone cannot produce a pass.

Evidence: `work/three-outfit-rig-physics/parry-interruption-20261015/matrix/`.
The separate Peony/Iris Out preflight also passes with two native observations.
Both commands and the final read-only restoration check completed successfully;
no diagnostic process remains running.

| Appearance | Iris Out | Come Closer |
| --- | --- | --- |
| Black Pearl | Pass | Pass |
| Planet Diving Suit | Pass | Pass |
| Skin Suit | Pass | Pass |
| Vacation Bikini | Pass | Pass |
| Casual Knitwear | Pass | Pass |
| Midsummer Alice | Pass | Pass |
| Peony | Pass | Pass |
| Unholy Genessa, Eve skin | Pass | Pass |
| Unholy Genessa, Genessa skin | Pass | Pass |
| Commander White | Pass | Pass |

The existing guarded action tools now support `--action parry`. They verify the
actual Q mapping before sending input, require early release of the same facial
performance token, observe the native parry, and wait for it to finish. Health,
Resolve, equipment, saved state, position/view, CINE and focus are guarded or
checked for restoration. The parry must leave neither an active parry ability
nor a gameplay input block. No enemy was sought or spawned.

The offline verifier checks raw ability observations and rejects six corrupted
records: playback still active, natural completion, missing native action,
parry still active, input still blocked and changed Resolve. The existing
twenty-case Genessa-ability record also passes `--check-only` with the extended
verifier, preserving its seven negative controls.

The parent restores all outfit memories and original Peony customization. A
separate read-only `final-live-restoration.json` verifies fourteen conditions:
completed matrix, original player, mapped core and hash, appearance, complete
saved state, CINE, position/view, health, Resolve, inactive facial performance,
released input, zero active/cached clones and restored original idle playback.
The saved `eve_idle_702` choice is available and engaged again.

The process remains PID 1944353 with core
`5f04a31900c8e323a27f6ed5650649595015b6dea6600f6682db582dc8fcd619`.
No production core, cooked assets or save progression was changed by this test.
This covers parry initiation and facial-playback interruption. It does not
establish incoming-hit deflection, riposte, damage response, Faithful-form
behavior or continuous cloth/contact quality. Those remain separate coverage.
