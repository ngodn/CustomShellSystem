# Non-CSS character guide: applicable findings

Reviewed 2026-09-21. The user supplied an auto-generated transcript under
`docs/development/non-css-modding-guide/`, titled
`[English (auto-generated)] How to Mod Mortal Shell 2 – Full Modding Guide Unreal Engine [DownSub.com].txt`.
The original video URL and exact demonstrated assets are not supplied. Treat
this as a reference workflow, not independent verification of our character.

The guide replaces Proxima with another character. Its useful steps are:

- Lines 351-449: match shoulders, elbows, forearms and fingers before applying
  the posed mesh and rebinding. This strengthens the case for inspecting the
  full arm/hand reference alignment rather than only finger closure.
- Lines 450-593: transfer weights, exercise joints, then remove unintended
  influences and smooth sharp transitions. Our read-only audit found original
  arm twist influences collapsed onto single main arm bones. See the
  [foundation comparison](game-foundation-comparison.md#next-binding-defect-to-isolate).
- Lines 924-973 and 1173-1195: reproduce the original Skeleton path in the
  authoring project and omit the replacement Skeleton from the package. That
  workflow relies on the game's Skeleton at runtime. It is consistent with
  MoreBeaute using the game Skeleton directly, and differs from CSS shipping
  its own separate compatible Skeleton.
- Lines 67-71: export sockets as bones. An exported armature can therefore
  contain attachment records that were not raw bones in the original Skeleton.
  Audit provenance and attachment resolution before treating such an export as
  the authoritative game Skeleton. This is a reason to inspect our inputs,
  not proof of how every historical CSS bone was created.

Adapt the tutorial to the CSS requirements. It rescales body proportions and
deletes shape keys while preparing a simple replacement. CSS must preserve Eve's
proportions, original morphs, modular garments and secondary-motion chains.
Pose alignment and binding checks remain useful without copying those steps.
The tutorial also avoids hair transparency; that does not replace CSS's required
hair/material workflow. Its final mention of jiggle/Kawaii physics defers those
subjects to another video, so this transcript supplies no solver implementation.

The supplied archive contains `MortalShell2-5.6.1.usmap`, 2,545,713 bytes, SHA-256
`235fd23b08c200442e1ad488843c123bdd044fbfd451f6acfed961ab166c39ee`.
The mapping currently used by the verified exporter has SHA-256
`b4b8b4916ff235e514b743c3b02981997b053fdc3ffea67da58d0fecada3dc9b`.
These are different mapping files; the archive has not been installed or
substituted for the pinned mapping. No evidence here establishes that it is
better for the currently installed game build.

The original transcript/archive remain user-provided reference files. This
assessment does not alter them or adopt the tutorial as a new CSS standard.
