# Cloth in the paused wardrobe

The user reported rigid pieces behind Genessa's shoulders in CSS while gameplay looked correct. [The original side view](../work/preview-cloth-side-before.png) reproduced it. We compared paused cloth ticking, missing post-process animation and unsupported idle bones. Live reflection showed both gameplay and preview had their post-process instance, a Chaos clothing interactor, cloth enabled and blend weight 1. The separate cloth tick was paused.

Unreal maintains `USkeletalMeshComponent::ClothTickFunction` separately from the primary component tick. [ClothTickExtension's source](https://github.com/shq300/ClothTickExtension/blob/UE_5.7.x/Source/ClothTickExtension/Private/ClothTickExtensionBPLibrary.cpp) demonstrates the independent pause flag. CSS applies this principle in its native adapter without installing the editor plugin.

## Verified layout

The game cloth tick vtable is RVA `0x9203478`. Its diagnostic function at `0x436c010` references UTF-16 `[ClothTick]` at `0x93e3d58`; its context label is `SkeletalMeshComponentClothTick`. The live member was at component offset `0xbc0`, with its target at `+0x28`. Flag byte `+0x0a` changed from `0x0a` to `0x0b` on the preview; gameplay stayed `0x0a`.

CSS does not blindly write the observed member offset. It checks the image header, diagnostic signature and label, then scans a small region between reflected ClothingSimulationFactory and TeleportDistanceThreshold fields for the verified vtable. A unique match must target this component, or be null before first tick registration. The reflected FTickFunction.bTickEvenWhenPaused property changes the bit. Other layouts are refused. Only the disposable preview is modified; destroying it removes that changed tick.

## Verification

[After the fix](../work/cloth-side-after.png), hood and sleeves settle instead of projecting rigidly. [Live checks](../work/cloth-preview-lifecycle.json) cover regular Genessa, corrupted Genessa and KnightLady: animated preview bones, active preview cloth tick, unchanged world time and unchanged gameplay animation/cloth flags. [Active reload cleanup](../work/cloth-after-reload-closed.json) confirms visibility, input and pause restoration. [HIT2 samples](../work/packaged-hit2-inspect-b.json) passed after installing its package, and the user confirmed movement and attacks.

Run `python3 tools/check_preview.py` with the player safely in the world for the three-Beaute-variant regression. The screenshot comparison established the visual fix; the automated test checks the engine condition responsible. Temporary raw-memory/function-address probes were removed. Useful cloth flags remain in normal diagnostics.

This validates the observed defect and these appearances. It does not establish universal support for every procedural animation, custom physics system or third-party mod.
