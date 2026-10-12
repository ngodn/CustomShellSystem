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

`tools/diagnostics/capture_peony_motion_inputs.py` is prepared for the next live
check through the already installed CSSX interface. It reads main/post-process
instance identities, global and regional body settings and reset epoch three
times, bracketing each read with selection/instance checks. Its allowlist has
only player/property reads and animation-instance getters. Syntax and the
no-game guard pass; the guard leaves both request and output paths unchanged.
Live field capture remains pending. It needs no replacement core, preset write,
camera input or animation control. A useful next capture is with Firm selected
for both chest and glutes, Peony standing in the world. This will distinguish
saved selections from the values reaching the active rig; it does not by itself
measure jiggle amplitude or prove whether another solver also runs.

### Previous Eve and Genessa comparison, at the user's direction

The user declined the Firm capture request and asked to use the previous Eve
and Unholy Genessa implementation first. The live request is no longer pending;
do not ask them to repeat it. No game interaction or installed change was made.

Evidence: `work/three-outfit-rig-physics/peony-v2-motion-compare1`.
`comparison.json` pins the metadata and cooked readbacks. Peony's four body rig
controls exactly match all six preceding Eve variants. Its body and facial
meshes use the same secondary graph as Black Pearl and Planet Diving. Current
Eve and Genessa cooked body defaults match for region tuning, moments, angular
limits and maximum travel. Their recorded post-process paths each contain one
body rig. This does not prove that all active game animation layers have one owner.

The older validated V42 implementation differs from those current rigs:

| Setting | V42 | Current Eve/Genessa |
| --- | --- | --- |
| Chest angle limit | 6 degrees | 28 degrees |
| Glute angle limit | 10 degrees | 24 degrees |
| Chest travel limit | 1.5 cm | 11 cm |
| Glute travel limit | 1 cm | 5 cm |

The later `gemini-work/build_cr_body_jiggle.py` recipe contains the wider limits;
the cooked readbacks confirm those values independently. CSS Firm/Natural supply
frequency, damping and acceleration strength, not angle/travel limits. Reduced
rig-default moments are not automatically the live moments: the AnimBP/native
geometry inputs also supply moments and offsets.

`tools/diagnostics/compare_body_rig_response.py` runs retained native rigs with
identical public geometry/contact inputs and Firm/Natural regional values.
It compares current, current with V42 angle/travel limits, and V42, at 30/60/120
fps, including disabled-region cases. The pelvis input moves 10 cm sinusoidally
for two seconds, followed by two seconds at rest. The game is not involved.
The project uses read-only source content and writes only its private stage.

At 60 fps, peak left/right chest response in degrees:

| Rig | Firm | Natural |
| --- | --- | --- |
| Current | 3.242 / 3.242 | 8.810 / 8.810 |
| Current with old limits | 3.242 / 3.242 | 3.833 / 3.823 |
| V42 | 3.021 / 3.021 | 3.833 / 3.823 |

All 27 cases completed, disabled regions produced zero relative motion, and
source hashes remained unchanged. Current Firm chest peaks range from 3.122
to 3.243 degrees across the three rates. The fixture settles after the motion.
`response.json` retains samples, `command.json` the editor invocation, and
`exit.json` records exit 0. The editor reported 192 warnings, including unrelated
assets in the shared read-only content mount and denied editor preference writes;
it reported zero errors. This was not a clean package cook.

Conclusion: wider limits materially increase Natural's response in this fixture.
Tightening them does not explain or reproduce the user's Firm failure. Do not
install a limit-only change as a claimed repair. This direct rig fixture bypasses
CSS preset delivery, the AnimBP event/update path, animation layering and actual
gameplay motion. Those integration paths remain the investigation priority.
Keep Peony's accepted fit, materials and controls intact.

### Saved Peony animation-graph input handoff

The independent editor module at `tools/authoring-probes/body-inputs` tests the
next boundary, the saved Peony secondary AnimBP on actual normal/facial meshes.
Evidence is `work/three-outfit-rig-physics/peony-v2-body-inputs1`. The private
C++20 UE 5.6.1 module built successfully. Source content is mounted read-only;
the commandlet exited 0 with zero errors and two editor-preference write
warnings. It never accessed the game or saved assets.

The fixture writes each preset once at a two-second phase boundary, following
Firm, Natural, Off, Firm. Every frame compares the expected regional values
with both the AnimBP public inputs and the live body's RigVM inputs. It rejects
even a matching pair of overwritten values. It moves the component, so the
solver runs; simulation is not inferred from a static property read.

Both `SK_PeonyV2` and `SK_PeonyV2_Face` pass at 30, 60 and 120 fps, totaling
3,366 frames. Each graph contains three rig nodes and one seven-region body
node. `SeenUpdate` advances exactly once per frame. Solver substeps are 4, 2
and 1 respectively, except for initialization/preset resets. ResetCount only
changes at preset boundaries, with no repeated reset between them. Source
hashes remain unchanged. `verified2.json` includes timestep verification;
`negative-controls2.json` records rejection of duplicate updates, extra resets,
missing cases, native failure and a stopped simulation.

This rules out a broken public-input-to-body-rig binding in this saved source
graph under these conditions. It does not test CSS core's reflected writes,
performance adapter entry/exit, the game's native animation layers, gameplay
physics or actual idle tracks. The user's defect remains open. Continue at those
boundaries without changing the installed rig based on a passing editor fixture.

One code path to investigate is `PerformancePlayer::bind_layer` restoration:
it copies current CSS properties back to the source instance, then applies
physics captured at performance entry. If customization changes while that
performance is active or cleanup is deferred, the older snapshot could overwrite
new settings. This is a code-review lead, not a demonstrated explanation of the
user's Firm result. Normal UI actions interrupt automatic playback before
applying controls, so event ordering must be reproduced before calling it causal.

### Playback restoration correction

`peony-v2-body-restore1` now reproduces the stale-snapshot overwrite in an
isolated UE 5.6.1 instance-replacement fixture on both Peony meshes. Bouncy is
captured at entry, Firm is assigned to the replacement instance, and restoration
copies the current settings before applying the physics snapshot. The old
ordering ends at chest frequency 1.7, damping 0.28 and motion 1.8. Refreshing the
snapshot before replacement preserves Firm's 2.6, 0.65 and 0.6, as well as every
value in the three seven-region arrays. Editor exit is 0 with zero errors and
two preference-write warnings; source hashes are unchanged.

The CSS candidate now captures the latest physics settings immediately before
restoring the original graph, after the montage/gameplay deferral checks. It
uses the original graph's class for restoration and retains the existing
different-mesh path. Failed adapter initialization keeps the original snapshot
and controls: the current settings become eligible for preservation only after
the adapter has received its initial CSS controls and physics successfully.

This fixture uses actual Unreal instance replacement, but not CSS core or its
performance adapter. It establishes the overwrite mechanism, not the user's UI
event sequence or the cause of all reported jiggle. In particular, normal UI
changes can interrupt playback before applying the selected preset. The exact
runtime sequence and actual motion inputs remain to be checked. No new core,
rig or outfit has been installed. Preserve the accepted Peony package.

The production-configured Windows C++23 core builds successfully. Existing
performance-session, control, Astral-adapter and Astral-lifecycle tests pass
(4/4). These portable tests do not execute the reflected restoration callback.
The native fixture's `verified.json` records its result and the candidate build
hash; its commands, source pins and logs are retained. Rebuildable compiler
intermediates and editor DDC were removed under the cleanup record.

### Authorized live motion and loader checks

The user subsequently confirmed they are willing to do game checks, requested
hot reload and authorized automated checking. Core
`21f761aa35fd18619172db3e4b3b88318a42681bdcdac848412f45c3299acc37` was
installed with the game closed. Only the core selector/new DLL and the stale
loader-contract hash were changed. The loader binary matches the installed
beta.11 release, and all five contract source hashes match that release's commit
and the working tree. The earlier core selector and state are retained under
`peony-v2-body-live2/rollback`; existing outfit/Shared packages are unchanged.

After normal launch, the user left Peony in the world. The existing standalone
CSSX diagnostic bridge reads the running rig without requiring a developer CSS
core. `baseline.json` has three stable readings. The selected Bouncy chest and
Soft glute settings match the live regional arrays. This baseline does not
dismiss the user's earlier Firm/Natural report.

`check_peony_live_presets.py` temporarily applies Firm, Natural and disabled
chest/glute controls through the normal CSS customization service, with transient
settings and an exact restoration snapshot. It checks the AnimBP public arrays
and active RigVM arrays against each expected preset, three times per case.
`presets.json` passes nine samples on `ABP_Secondary`.
`playback-presets.json` passes nine more while the Iris Out `ABP_Eve` adapter is
active. Each has one body rig, plus hair and hand rigs. The main game's graph
uses `CTRL_Human_FullbodyIK`, not a second instance of the Eve body rig. These
observations narrow ownership but do not rule out every source of motion.
Both checks restored the full original customization; the playback check also
restored the original idle override. No gameplay or camera input was sent.

Natural's sample had extra resets (265, 277, 282 across the three readings).
Firm's readings held at 257, disabled at 283. A later five-sample read held at
361, with valid scale, no scale-change/teleport flag and matching reset epoch.
These fields were read across successive frames, not atomically, so the reset
cause is unproven. A separate ten-second CSSX frame sample measured 262 frames,
26.13 Hz, maximum 76.27 ms and two hitches. The rig's source resets above roughly
66.67 ms, making hitches a lead to test, not a demonstrated explanation.

The user-requested hot-reload trial used a uniquely named byte-identical core
with Peony stationary, inventory closed and no native gameplay montage.
`hot-reload1/result.json` confirms actual old DLL unmapping/new DLL mapping,
unchanged process and exact customization restoration. The three subsequent
`post-reload.json` samples are stable and complete. The core remains selected as
`css_core-peony-motion-21f761aa35fd1861-reload1.dll`. This validates this controlled
reload only; the historical animation-worker crash and other reload states
remain outside its evidence.

For the user's visual check, `visual-firm.json` retains the original snapshot
and verifies temporary Firm values on the live rig. Restore that snapshot after
the user responds. Visual acceptance of excessive motion is still pending.
