# Cloth-transfer build intermediate cleanup, October 14

Remove only `work/three-outfit-rig-physics/eve-surface-20261014/cloth-transfer/editor/Intermediate`.
This contains regenerable object files and the precompiled header from this
session's finished UE 5.6.1 commandlet build. Keep source files, the compiled
module, build receipts, previous-build evidence and all created asset data.

Before removal, require the successful build receipt, matching compiled module
hashes and no running compiler/build process for this project. Record the file
count, logical size, retained hashes and `df -h` before/after. Do not remove any
other project's intermediates or caches.
