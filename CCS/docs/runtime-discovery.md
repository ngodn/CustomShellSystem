# Runtime discovery and patch compatibility

User correction, 27 September 2026: CCS should read current game content dynamically, as CSS resolves live reflected data, rather than depend on a fixed list of 109 extracted records. The generated catalog is an offline research snapshot and test fixture. It is not the intended runtime source of truth. Loaded player selector reads are now verified in the first live probe. A bounded experimental [loaded-selector reader](loaded-moves-runtime.md) now publishes current observations without loading the extracted fixture at startup. The experimental menu now has a bounded [installation change monitor](content-monitor.md) that invalidates observations after disk-manifest changes. Full catalog discovery, eligibility, mounted-content/byte identity and patch-aware cache management remain unfinished.

## Source priority

1. Current live objects and their relationships: player ASC abilities, attack selectors, referenced montage objects, weapon/item definitions, item fragments and upgrade levels. Read the actual values and full object paths. Use selector references and tags to determine original slot roles; do not infer them from class-name suffixes or damage thresholds.
2. The game's cooked Asset Registry for discovering relevant assets that are not already loaded. The retained game header dump exposes GetAssetRegistry, IsLoadingAssets, GetAssetsByPath, GetAssetsByClass and GetDerivedClassNames. Confirm the interfaces and reflected output layouts in the running game before using them. Registry records provide discovery metadata; they do not provide every montage notify, stat fragment or gameplay detail without loading its asset.
3. A validated metadata cache for startup/UI convenience. Key it by installed game content identity and discovery schema. Store paths and metadata, never UObject pointers. Refresh on relevant object generations and invalidate when game content or required interfaces change. A cache cannot override conflicting live values.
4. Extracted snapshots for research, regression tests and comparison. Do not silently activate stale snapshot entries after runtime discovery fails.

The current generated snapshot's build string only identifies its extraction source. Comparing that string inside catalog.json does not detect the installed game's build or establish runtime compatibility.

## Reference patterns

CSS's native/src/engine.cpp resolves functions and properties by reflected name, checks their types/layouts, and caches handles by validated owner identity. OwnerGuard checks object-array index, serial and name; stale handles are resolved again. This accommodates many address and content changes while the interface remains compatible.

CSS does not automatically support every patch. docs/development/game-build-compatibility.md records the dye adapter needing updated native call-site/prologue verification after an executable change. Reflection compatibility and native binary compatibility are separate checks.

The retained CCS combat surface documents live Montage properties and the granted-ability/selector relationships. These are the starting point for player move discovery. Full enemy discovery additionally needs registry queries and per-asset skeleton, track, notify, trace and payload validation.

## Performance and failure behavior

- Resolve current player relationships when the player becomes available and on weapon, shell, ASC or world generation changes. Use guarded cached references for repeated use.
- Query asset metadata in limited batches and publish immutable catalog snapshots. Keep UObject access on the game thread and disk cache writes on the background worker.
- Never scan all UObjects, enumerate every asset or rebuild the catalog on each combat tick. UObject enumeration covers only objects already in memory and is not a complete installed-content index.
- Use a verified asynchronous loading path for assets needing deeper inspection. A synchronous LoadAsset_Blocking call cannot become safe merely by processing one asset per tick. Do not cold-load unknown enemy assets during combat.
- Check required property types, array inner types, function parameters and struct layouts before reading/writing. If a patch changes a contract, disable the affected feature and expose the reason; do not reuse historical offsets.
- Detect the actual installation/content identity and re-probe after changes. Engine version alone is insufficient because content hotfixes can retain the same engine version/build label.
- Build/content detection, schema validation and cached discovery are separate from a successful in-game attack test. Newly discovered moves remain unavailable until their runtime compatibility checks pass.

Primary documentation consulted: [UE4SS UObject enumeration](https://docs.ue4ss.com/lua-api/global-functions/foreachuobject.html), [Epic Asset Registry](https://dev.epicgames.com/documentation/unreal-engine/asset-registry-in-unreal-engine). Game-specific registry access and loader behavior remain live verification tasks.

## Prepared first probe

[Loaded combat probe 01](discovery-probe-01.md) is a separate, idle-by-default build that reads current grants and selector references when triggered. Host tests and cross-builds validate its CPU-side behavior, and installation is verified, and the first live capture completed. It read 137 grants and followed four current Clockwork Scythe selectors. Full registry discovery and installed-content cache invalidation remain pending.
