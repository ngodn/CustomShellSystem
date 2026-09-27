# Eve idle gameplay acceptance

Candidate: E6. Each clip has 101 source frames at 30 fps. All 14 use the provisional common-body mapping documented in `eve-idle-expansion.md`; unresolved source helper indices remain archived.

| Source clip | CSS name | Offline review | Game world, transitions, weapons | Six outfits |
|---|---|---|---|---|
| 700 | Eve Standing Poise | E5: 51 rendered/contact samples | Pending | Pending |
| 701 | Eve Kneeling Poise | E5: 51 rendered/contact samples | Pending | Pending |
| 702 | Eve Seated Recline | E5: 51 rendered/contact samples | Pending | Pending |
| 703 | Eve Kneeling Bow | E5: 51 rendered/contact samples | Pending | Pending |
| 704 | Eve Low Lean | E5: 51 rendered/contact samples | Pending | Pending |
| 705 | Eve Deep Kneeling Bow | E5: 51 rendered/contact samples | Pending | Pending |
| 706 | Eve Reclining | E5: 51 rendered/contact samples | Pending | Pending |
| 707 | Eve Forward Lean | E5: 51 rendered/contact samples | Pending | Pending |
| 708 | Eve Forward Fold | E5: 51 rendered/contact samples | Pending | Pending |
| 709 | Eve Kneeling Hands Back | E5: 51 rendered/contact samples | Pending | Pending |
| 710 | Eve Arched Stretch | E5: 51 rendered/contact samples | Pending | Pending |
| 711 | Eve Crouched Hands Together | E5: 51 rendered/contact samples | Pending | Pending |
| 712 | Eve Seated Tuck | E6: 101 contact samples; 51 rendered samples | Pending | Pending |
| 713 | Eve Side Kneel | E5: 51 rendered/contact samples | Pending | Pending |

For each clip, record the installed package hashes, outfit, recording path, weapon state and result. Verify idle entry, loop, movement, sprint, attack, aim, dodge and damage interruption, then weapon restoration. Include menu exit and profile persistence. A decoded asset or passing offline pose does not establish gameplay acceptance.

Across the six outfits, review secondary motion and clothing through the full pose range. Bone compatibility is verified in `work/eve-idle1/six-rig-check.json`; this does not prove skinning or physics behavior.

Existing Eve Default Idle keeps ID `eve`. Its saved choice and existing movement choices must survive installation. Preserve CSSX disabled state and verify the prior cinematic-camera and combat behavior.
