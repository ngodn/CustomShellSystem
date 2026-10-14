# Attachment limits in CSS

**October 14 follow-up:** the current installed core passes three Peony
facial/restoration cycles with 129 real registered direct children. The 73
temporary empty components are removed and original handles/parents/sockets,
settings and camera restore. See [the live evidence and limitations](2026-10-14-attachment-limit-live.md).
The staged-only status below records the original October 11 candidate.

The recorded accessory-recovery and idle-facial errors both reach
`attached_children`. Its fixed 128-child limit also affects performance weapon
hiding. `AttachmentFollower` separately stops at 128 repaired children, and
`AttachmentOffsets` stops at 64 corrected children. These are CSS restrictions.

The local UE 5.6.1 `SceneComponent.cpp`, `GetChildrenComponents` at line 2146,
copies the direct attached children when recursive traversal is disabled. It
does not impose a 128-child limit. The retained UE4SS SDK exposes the array
count through `FScriptArrayHelper` and the registered object population through
`FUObjectArray::GetNumElements`. The latter reads the actual object-array count,
not the size in bytes returned by `GetObjectArraySize`.

The production snapshot now accepts valid arrays above 128. It retains the
reflected array/object layout checks, rejects negative counts or counts larger
than the registered object population, checks non-null storage, creates weak
handles, validates scene-component type and excludes children whose parent
changed. It reserves the returned count once. The two owned-record vectors no
longer have fixed outfit-size limits. Their existing stale-record removal and
restore-before-destroy behavior remain in place. Material limits and unrelated
character-data bounds are unchanged.

`attachment_children_tests.cpp` includes the production snapshot implementation
against reflected-call fixtures. The original code fails with `Too many
character attachments` at 129 children. The changed code passes 0, 64, 128, 129,
257 and 4097 children without truncation or reordered handles, plus stale-handle,
reparenting, invalid-count, missing-storage, wrong-component and wrong-layout
checks. This does not simulate real UE object registration or full facial and
accessory lifecycle behavior.

The Windows Release core builds. Seven portable tests pass: attachment snapshot,
MISC attachment classification, recovery, animation runtime, facial performance,
socket fit and inventory motion. `work/three-outfit-rig-physics/attachment-limit1`
retains the candidate DLL, source hashes, patch and test output. It is staged,
not installed. Peony refit2 still uses the previous core.

Before live acceptance, verify recovery and facial playback with more than 128
direct children. Also inspect component ownership/count across repeated facial
and outfit changes: removing the limit does not prove that the original high
count was free of leaked components. Keep action interruption, aiming, Genessa
forms/doubles, customization, unload/restoration and frame-time checks pending.
