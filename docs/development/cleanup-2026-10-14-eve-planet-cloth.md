# Planet Diving cloth repair cleanup, October 14

Only this session's new `eve-surface-20261014/planet-ordered` and
`eve-surface-20261014/cloth-transfer` work is in scope.

Prelisted removal groups:

- `planet-ordered/cook/out/prefix`: disposable Wine prefix after terminal successful cook and an absent container.
- `planet-ordered/cook/project/Content`: duplicate cook inputs, after comparison with retained editor sources and the input receipt.
- `planet-ordered/cook/project/Saved/Cooked`: regenerable first cook, after its mesh/skeleton shipping payloads match retained `pack/stage` files and the package/source comparisons pass. Keep the failed unit-length result for the remaining zero tangent.
- `cloth-transfer/planet-cook/out/prefix`: disposable Wine prefix, only after terminal successful paired cook and absent container.
- `cloth-transfer/planet-cook/project/Content`: duplicate paired cook inputs, only after exact comparison with the retained editor assets and input receipt.
- `cloth-transfer/planet-cook/project/Saved/Cooked`: regenerable paired cook output, only after all shipping payloads match retained `planet-pack/stage` and source, tangent and cloth/config comparisons pass.

Keep the accepted sources, reordered import/receipt, candidate editor assets,
binding intermediates, compiled transfer module, useful DDC, packaged data,
readbacks and all pass/fail evidence. No unrelated or unknown directories are
included. Record actual removals and `df -h` before/after in the work folder.

The first three groups were removed after verification, totaling 1,462,198,829
logical file bytes. The failed unit-length report remains. Receipts are
`planet-ordered/candidate-cleanup-plan.json` and
`planet-ordered/candidate-cleanup-completed.json`. Their final `df -h` reports
root 94G and theark 8.1G free. Paired cook cleanup has not run yet.
