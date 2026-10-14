# Alice cloth pair cleanup, October 14

Remove only these groups after the private pair's cook, source readback and
cloth/binding comparisons pass, and its cook container is absent:

- `build/cook-scratch/alice-cloth/out/prefix`: disposable Wine prefix on the root drive.
- `build/cook-scratch/alice-cloth/project/Content`: duplicate cook inputs checked against retained editor assets.
- `build/cook-scratch/alice-cloth/project/Saved/Cooked`: regenerable cooked tree whose shipping payloads remain in `alice-pack/stage`.

Keep sources, useful DDC, readbacks, containers, recipes and all pass/fail
receipts. `cloth-transfer/alice-cook/project` and `out` link to these root
scratch directories. Verify the exact destinations and retained hashes before
and after removal. Record `df -h` for root and theark. Unknown folders and other
projects are outside this list.
