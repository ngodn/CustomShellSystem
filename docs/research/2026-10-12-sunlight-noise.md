# Moving sunlight noise: evidence and capture

Status: high priority, now also reported intermittently by the user on this
setup. On October 13 the user said they sometimes see broken shadows resembling
Vistani's report. This supersedes their October 12 answer, "Only in Vistani's
report so far". The local occurrence has not yet been captured or matched to a
specific rendering pass. No renderer setting or material has been changed for
this report. A reproducible visual comparison is still needed before proposing
a MISC workaround. Continue the active CW foot investigation first.

The user supplied Vistani's report of noise outdoors in direct sunlight during
movement, following installation of more CSS outfits and Shared Assets. The
reported driver, hardware-ray-tracing and denoiser experiments are not controlled
comparisons available to this session. They neither establish nor exclude a
cause here.

The [reported image](https://i.imgur.com/XiAVEbK.png) was retrieved and inspected.
It shows a blocky dark/light pattern across exposed thighs and an arm; the pale
garment does not show the same obvious pattern. This static image cannot verify
motion dependence or identify the rendering pass. Do not equate it with Peony's
previous broad dark/bright appearance concern.

The original image and URL/hash receipt are retained in
`work/three-outfit-rig-physics/sunlight-noise1`. Its SHA-256 is
`83a9718e75a65479701dd9097268bc1e838a021ca1758e39acfe0704633154b5`.

## Read-only live baseline

`tools/diagnostics/capture_reflection_settings.py` uses the existing CSSX
reflection bridge and UE's string console getter. It invokes no console setter,
appearance change or gameplay input. A deliberately nonexistent variable must
return an empty string; missing real variables are recorded as unavailable.

The first capture, `sunlight-noise1/settings.json`, completes with 20 available
variables. Two consecutive reads match. The player reference, process and CSS
customization also match before/after. These are this machine's values, not
Vistani's configuration, and are not synchronized to a screenshot.

| Console setting | Captured value |
| --- | --- |
| Reflection method / dynamic GI method | 1 / 1 |
| Ray tracing / Lumen hardware ray tracing | 1 / 1 |
| Ray-traced shadows / virtual shadows | 0 / 1 |
| Shadow denoiser / temporal accumulation | 2 / 1 |
| Directional virtual-shadow rays / samples per ray | 4 / 2 |
| Contact shadows | 1 |
| Lumen reflections allowed / temporal / bilateral | 1 / 1 / 1 |
| Reflection history maximum frames | 12 |
| Anti-aliasing method / screen percentage | 4 / 58.328 |
| Shadow / reflection / GI scalability | 2 / 2 / 2 |

Console values do not establish the final blended view, which GPU passes execute,
which upscaler is active, or whether the screenshot symptom is present. The
capture does not verify camera stability. There is no visual pass/fail result.

## Boundaries for the next comparison

After the motion relaunch, `settings-after-motion-relaunch.json` records the
same 20 console values and unchanged process/player/customization across the
read-only capture. `peony-world-baseline.jpg` was obtained through the existing
CSSX Steam screenshot bridge without gameplay or camera input. Its receipt is
`peony-world-baseline.json`. The rear three-quarter view beside outdoor stairs
does not show an obvious matching blocky pattern on the small visible skin
areas. It is a single image, not a matched moving/standing comparison, and was
not synchronized to the console capture. Together with the user's clarification,
this leaves the reporter's issue open rather than establishing a local fix.

Epic's [UE 5.6 virtual-shadow documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine?application_version=5.6)
describes sampling noise in soft shadow edges and its dependence on directional
ray counts. Its [Lumen documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine?application_version=5.6)
also describes a reflection-quality/noise/GPU-cost tradeoff. These establish
multiple rendering paths to distinguish, not a diagnosis from the image.

The retained UE 5.6.1 source `Renderer/Private/LightRendering.cpp`, lines 82-105,
defines shadow-denoiser value 2 as the global denoiser interface, which a plugin
may override. A captured value of 2 does not identify that implementation or
prove a broken denoiser. The local source is an API reference, not proof that
the shipped game uses an identical renderer.

Next obtain a repeatable moving/standing comparison under the same sunlight,
appearance and camera. Preserve the reference settings and original materials.
Once the symptom is reproducible, isolate skin/material, direct shadow and
reflection contributions one at a time. Compare native appearance and CSS
appearance, then package combinations if evidence warrants it. Restore each
temporary change and measure performance and visual costs before considering a
user-facing toggle. No speculative denoiser override is part of the prepared
Peony motion trial.
