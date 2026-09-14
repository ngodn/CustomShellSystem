# Recording CSS media locally

`tools/css_capture.py` drives the development build through the existing request
file. It is a local filming tool, not part of the runtime ZIP or outfit format.
The scripts currently target this workstation's game path, Steam screenshot
account, DP-1 display and 0.2.0 output directory. Adjust those constants when
using another machine. Use Python 3.14.

## Setup

Build and stage the Inventory development core with `tools/inventory_dev.py`.
Its request handlers are compiled only with `CSS_INVENTORY_DEV`. Close Inventory
before starting a world camera sequence. Keep the player somewhere safe.
World filming does not pause enemies or protect the player from damage.

The helper needs Hyprland, gpu-screen-recorder with H.264 encoding, and Steam's
screenshot API. Published video copies are encoded separately with ffmpeg.
It records the fullscreen game on DP-1 at 60 fps with desktop audio, without
microphone input. Keep other audio applications quiet. The display guard rejects
a take when focus leaves the game. It is not a privacy barrier against transient
notifications or overlays; review each take before publication.

## Camera and actions

`inventory_capture_row` selects the Shell, Color or Templates section and its
row. `inventory_capture_motion` animates the native menu camera through yaw,
zoom and framing targets. Normal `select`, `palette`, `color`, `save_look` and
`load_look` requests use production CSS state operations.

World sequences use `inventory_cinema_start`, `inventory_cinema_move` and
`inventory_cinema_stop`. The camera follows the player's world position.
Coordinates are yaw, pitch, distance and vertical framing. Allow ten seconds
for the game's camera handoff before recording the first frame.
The helper restores the prior camera and HUD, stops on player changes or
Inventory opening, and times out after thirty seconds without a move command.
Always stop in a `finally` block. Restore any demonstration templates, favorites
and colors after filming.

## Quality checks

Keep originals under ignored `work/media-v0.2.0/originals/`. Encode a separate
copy for GitHub, preserving 60 fps:

```sh
ffmpeg -i original.mp4 -vf scale=1920:1080:flags=lanczos -r 60 \
  -c:v libx264 -crf 20 -preset slow -pix_fmt yuv420p \
  -c:a aac -b:a 192k -movflags +faststart published-1080p60.mp4
```

Check duration, dimensions, frame rate and file size with ffprobe. Decode the
whole result to check for errors. Review motion and frames throughout the video,
including transitions. A 60 fps stream does not prove sixty unique game frames
per second. If a file exceeds GitHub's 100 MiB regular-file limit, split it at
natural chapter boundaries or use a two-pass bitrate budget. Keep the original.

An X11 window capture was tested but replaced with display capture at the
author's request. The LSFG library was present in the game process; that alone
does not prove frame generation caused a recording defect. Do not describe the
comparison as an established LSFG bug.

References: [FFmpeg inputs](https://ffmpeg.org/ffmpeg-devices.html),
[FFmpeg encoding options](https://ffmpeg.org/ffmpeg.html), and
[lsfg-vk configuration](https://github.com/PancakeTAS/lsfg-vk/wiki/Configuring-lsfg%E2%80%90vk).
