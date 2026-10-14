# Facial playback interrupted by actual aiming

All twenty cases pass: Iris Out and Come Closer each release facial playback
when actual aiming begins, across seven Eve variants, both Unholy Genessa
variants and Commander White. Drawn Burn remains visible and stowed Burn stops
rendering. Peony, full saved CSS settings and the CINE model restore afterward.

Final evidence:
`work/three-outfit-rig-physics/outfit-aim-interruptions-20261015-3/`.
This incorporates thirteen verified cases from the `-2` directory and runs the
remaining seven. Saved settings and the installed core must match before reuse;
`verified.json` identifies and hashes each case's original records. No accepted
case was rerun merely because the desktop lost focus.

| Outfit | Variants | Clips per variant | Passing cases |
| --- | ---: | ---: | ---: |
| Eve | 7 | 2 | 14 |
| Unholy Genessa | 2 | 2 | 4 |
| Commander White | 1 | 2 | 2 |

All twenty stop with `A gameplay action is blocking input`, at clip times
1.2856 to 1.5415 seconds after starting at 0.5 seconds. These are clip-clock
positions, not measured input latency. Every child exits zero, needs no forced
performance stop in cleanup, preserves health/Resolve and infusion, and
releases all sampled keys. Position, view target and control rotation match
exactly; actor rotation uses the established 1e-8-degree roundoff tolerance.
The resumed parent also exits zero. The independent verifier rejects both
still-active and natural-end negative controls.

## Method

`tools/diagnostics/check_outfit_aim_interruptions.py` visits the seven Eve
variants, both Unholy Genessa variants and Commander White. Each runs Iris Out
and Come Closer through the public performance service. Once full playback
weight and facial ownership are observed, the child diagnostic sends actual
Ctrl aim down/up through the existing focused developer input helper.

The child checks that the same performance token stops before the clip ends,
with a gameplay interruption reason and facial ownership released. It also
requires the drawn Naylshotte and active Burn effect to render, followed by
hidden stow without recent effect rendering. It does not fire, move the player,
change the camera or write health, Resolve or infusion values.

The parent uses the existing outfit-switch fixture's ten-variant matrix and
memory restoration. It temporarily selects the body-only Eve idle so an outfit's
automatic dance does not take ownership before the requested clip starts.
Original per-variant and outfit-level customization memories are restored
through normal CSS commands. Complete saved CSS JSON must match afterward.

Core: `css_core-development-d7cb0635d112f5fa-item-visibility.dll`, SHA-256
`d7cb0635d112f5fae73f67bc261c81d07df07eb0fef8797d786cc5d55191072f`.
Host scripts use Python 3.14.7. No runtime binary, outfit or shared asset is
rebuilt or installed for this test.

## Earlier attempts

`peony-aim-interruption-20261015/` passes the initial Iris Out case on Peony.
The clip reaches full facial playback and stops at 1.2835213989 seconds with
`A gameplay action is blocking input`. Draw, stow, saved state, CINE, view,
infusion and input cleanup pass.

The first matrix attempt, `outfit-aim-interruptions-20261015/`, stops at Black
Pearl's child preflight. The wrapper had left the outfit's automatic idle
selection active; a performance was already running. The corrected wrapper
uses the same temporary body-only idle as the established lifecycle fixture.
That first attempt preserves saved CSS state and player/view. Its CINE comparison
incorrectly included the RPC request identifier. The executed recipe is retained.

The corrected idle setup reaches thirteen passing cases in `-2`, then the
focused-input preflight stops before Peony's Come Closer case. It sends no input
for that rejected case. Peony, saved settings, control values and view restore.
The user's requested normal shot is tested separately in
`sidearm-user-shot-20261015/` and passes; see the [shot record](2026-10-15-sidearm-aim.md).
The resumed `-3` run independently verifies and reuses the thirteen completed
cases, then completes the remaining seven.

Both later wrappers archive CINE responses. Their informational
`cine_model_equal=false` also compares the entire RPC envelope, including its
unique request ID. The actual final model payloads match exactly. The independent
verifier checks `result` separately and records `cine_model_equal=true`;
the reusable wrapper now makes that same comparison. Original runtime receipts
remain unchanged. Persisted CINE settings and control-value restoration already
passed during execution, so this correction requires no repeated gameplay.

## Verification and scope

`tools/diagnostics/verify_outfit_aim_interruptions.py` checks raw token, phase,
time, expression, socket, hidden/rendered, resource and restoration records. It
requires exactly one case per variant and clip. Two deliberately altered copies,
one still playing and one ending naturally, must fail the verifier.

This tests aiming interruption in Faithful form. It does not replace visual
facial-track inspection, native-human compatibility, Stray/doubles coverage,
attack/combo/dodge/interaction cancellation, damage or moving contact checks.
The prior draw/fire/stow test remains the firing evidence. Matched performance
measurements and the full stable-release goal remain open.
