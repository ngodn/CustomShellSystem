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
| Flowing trim on/off, speed and unaffected fabric/skin/stockings | User check requested |
| Four palettes, removable parts and stocking layers | Portable/data checks pass; native visual checks pending |
| Body-shape endpoints and combinations | Cooked morph/weight checks pass; complete native sweep pending |
| Facial playback and Faithful/Stray doubles | Catalog, shader and source-binding checks pass; live checks pending |
| Profiles and restoration | Portable V2 upgrade checks pass; live save/reload pending |
| Continuous secondary motion, interruptions, resets and frame cost | Pending |

The captured log also contains a Genessa form request followed about 15.7 seconds
later by `Genessa form switch cancelled or timed out`. This is a separate
observation, not proof of the queued cross-save cause or a new Peony defect. The
source log tail is retained. Subsequent runtime status returned to `Settings
updated.` with no maintenance or overlay error.

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
