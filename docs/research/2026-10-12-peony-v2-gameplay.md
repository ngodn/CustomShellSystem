# Peony V2 gameplay checks

The user reports **"Fit looks right in both"** after being asked to check the
installed V2 outfit in CSS and while walking in the world. This accepts the
obvious detached/distorted fit in those views. It does not establish every body
shape, gait, action, reset or customization combination. Small unobtrusive
clipping remains deferred under the user's earlier direction.

The installed candidate is `peony-v2-package3`, with core
`94bb1d08bc7d6dd4181e21d4605a3b87cfb7f6046c90f35194f0e090291c423b`.
`work/three-outfit-rig-physics/peony-v2-gameplay1/fit-acceptance.json` records the
question, answer and acceptance limits. Runtime snapshots taken after the answer
confirm the applied appearance is `eins0fx.seduxtress/peony` with the V2 mesh.
They are not synchronized measurements of the user's visual check. No character
or camera input was sent.

| Check | Current evidence |
| --- | --- |
| Obvious fit/distortion in CSS and world walking | User accepted |
| Flowing trim with glow/speed above zero; unaffected fabric/skin/stockings | User answered "Works in both" for CSS and world |
| Four palettes and removable clothing | User answered "Everything restores correctly" after the requested palette/parts/profile check |
| Body-shape endpoints and combinations | Cooked morph/weight checks pass; complete native sweep pending |
| Iris Out face/eyes, movement or attack interruption, current-form doubles and their outfit/glow | User answered "All work"; the other form remains unverified |
| Appearance under bright light, shadow and backlighting | User now says "so far now its fine" on the unchanged build; accepted for now, earlier observations retained |
| Profiles and restoration | Portable upgrade checks and user save/change/reload check pass for colors, visible pieces and glow |
| Continuous secondary motion, interruptions, resets and frame cost | New excessive-jiggle report despite Firm/Natural; investigation required |

The captured log also contains a Genessa form request followed about 15.7 seconds
later by `Genessa form switch cancelled or timed out`. This is a separate
observation, not proof of the queued cross-save cause or a new Peony defect. The
source log tail is retained. Subsequent runtime status returned to `Settings
updated.` with no maintenance or overlay error.

## October 12 lighting report

After accepting fit, flowing trim and Iris Out/current-form doubles, the user
reported that the appearance looks good in favorable lighting but "burned or
washed out" with insufficient lighting. The supplied rear/backlit view is
`/home/eins0fx/Pictures/screenshot-2026-10-12_14-56-08.png`; the front/lit view is
`/home/eins0fx/Pictures/screenshot-2026-10-12_14-55-53.png`. Fabric becomes very
dark in the former; the latter loses visible detail in bright fabric and skin.
Different view directions and illumination prevent these screenshots alone
from isolating a material defect, exposure behavior or the environment's fill.

The report reopened lighting quality, not the accepted geometry. After reopening
the unchanged build, the user says "so far now its fine". Keep the current
materials and resume the remaining Peony checks, preserving accepted fit, facial
playback, flow mask and current-form doubles. This accepts the current appearance
for now; it does not establish an all-outfit comparison or a repaired cause.
See [lighting investigation](2026-10-12-peony-v2-lighting.md). No lighting repair
or new installed candidate is implied.

## Cooked secondary-motion bindings

`tools/eve-fit/audit_peony_v2_motion_bindings.py` reads the hash-pinned accepted
Eve graph readback, installed shared metadata and V2 cooked character streams.
`work/three-outfit-rig-physics/peony-v2-motion-bindings1/verified.json` records the
result and input/recipe hashes. The two normal/facial meshes have identical
cooked position/weight/index streams, matching hierarchy and secondary bindings.
Their 44 nonempty sections use 46 material slots. All named solver targets
exist. The idle-playback adapter preserves its source graph's control types,
defaults, bindings, pose flags and writer order: body, hair, hand correction.

| Region | Cooked owner and direct skin influence |
| --- | --- |
| Chest | `EveSB/CR_Body`, chest helpers drive body, jacket, corset, upper trim and relevant stocking layers |
| Glutes | `EveSB/CR_Body`, glute helpers drive body, thigh fabric, hip bands/straps, underwear, corset/lower trim and leg stockings |
| Thighs | `EveSB/CR_Body`, thigh-twist helpers drive legs, thigh fabric and leg stockings |
| Abdomen | `EveSB/CR_Body`, belly helper drives body, underwear, corset/upper trim and relevant stockings |
| Hair | `EveSB/CR_Hair`, the named ponytail/bang/side chains drive the retained hair sections |
| Hands | `EveSB/CR_Hand`, compiled correction retains its graph position; this named-array audit does not decode its literal bone targets |
| Other fitted pieces and ornaments | No additional named body/hair solver influences in this audit; ordinary skeleton weights remain |
| Cloth | Neither V2 mesh contains a separate cloth asset |

The corrected source's thigh and calf fabric is body-following, as recorded in
the V2 source study. The rejected source's free hanging-panel configuration is
not transplanted. This inventory does not waive secondary-motion requirements
or establish visible motion/contact, solver reset behavior, live branch
selection or performance. Any remaining loose-detail simulation needs actual
motion evidence before changing the accepted fit. Existing solver contact and
reset fields are retained in the report and the earlier
[secondary-motion ownership record](2026-10-08-secondary-motion-ownership.md).

## Passive engine timing sample

After the provisional lighting acceptance, the existing CSSX development
interface captured ten seconds of engine tick timing without input, camera,
appearance or settings changes. Evidence is
`work/three-outfit-rig-physics/peony-v2-gameplay1/passive-frame-sample1.json`,
which embeds and hashes the original `fps_probe.py` capture. The same game
process and applied Peony appearance are present before and after, with CSS's
menu inactive. CSS animation and maintenance errors are empty at both endpoints.

The sample contains 346 engine intervals: mean 28.94 ms, median 26.46 ms,
p95 44.44 ms and p99 88.38 ms. There are 12 intervals above the tool's hitch
threshold of twice the median. These are observed engine callback intervals,
not GPU presentation timing or isolated Peony cost. CSSX's measured core phase
averages 0.279 ms in this sample, but does not cover CSS, game logic, rendering
or all possible waiting costs. CSS animation's cumulative counters are also
not a measurement of this ten-second window.

No baseline outfit, fixed view, verified action sequence or noise-floor repeats
were captured. Do not call this a performance pass, a Peony regression or an
attribution of the hitches. The full motion/performance acceptance remains open.

## Native material values and profile acceptance

`tools/diagnostics/capture_peony_materials.py` reads the live player's garment
and stocking material instances through CSSX without changing selections.
`live-materials1.json` under the gameplay evidence directory captures 42 dynamic
and parent instances, with the same process, player, Peony mesh, slot objects
and customization before and after. This brackets a sequential capture, not
an atomic snapshot. The active palette was `black_white_floral`.

`tools/diagnostics/check_peony_materials.py` compares inherited runtime values
against package3's manifest plus the captured selections. All 240 unique
parameters across 23 slots pass, covering 122 control bindings. In-memory
negative controls reject an incorrect scalar, vector, texture and changed
selection. Raw evidence, checked values and negative results are retained in
`live-materials1.json`, `live-materials-checked1.json` and
`live-materials-negative-controls.json`. These checks do not prove visibility
or shader output by themselves.

The user separately answered "Everything restores correctly" to the four
palette/removable clothing/profile-save/change/reload check. The acceptance
record is `customization-acceptance.json`. Keep it distinct from the motion
failure reported afterward.

## Body motion report after customization testing

The user reports excessive jiggle despite trying Firm and Natural and suspects
duplicated motion. The last status captured before game exit has Bouncy chest
values and Soft glute values, but the CSS log confirms earlier Firm and Natural
applications followed by stronger presets. The final values therefore do not
explain or dismiss their observation. Keep this as an unresolved defect.

Evidence is `work/three-outfit-rig-physics/peony-v2-motion-live1/last-status.json`
and `preset-log-tail.txt`. A subsequent read-only player request timed out; a
process check then confirmed the game had exited. No live rig fields were
captured by that request, and no preset, slider or animation setting was written.

Next distinguish three predictions: if propagation fails, saved preset values
and the active post-process region arrays will differ; if motion is duplicated,
more than one active animation/physics path will drive the affected bones; if
the solver response is excessive, one correctly configured owner will still
produce the reported motion. The earlier cooked ownership audit cannot decide
these runtime cases. Existing body-control and solver probes are references,
not permission to reuse old results as Peony motion acceptance.
