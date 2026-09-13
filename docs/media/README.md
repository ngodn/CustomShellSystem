# CSS gallery media

The `v0.1.1/` folder holds nine screenshots from gameplay, the wardrobe and
the inventory, plus a web copy of the 63-second wardrobe recording captured
on 14 September 2026. These files are
tracked so the README and external mod pages can link to them. They are not
part of the runtime ZIP.

## Nexus page header

[css-nexus-header-v2.png](v0.1.1/css-nexus-header-v2.png) is generated promotional
artwork combining the CSS identity with Seductress and Marrow Keep references.
It includes the title and `by _eins0fx` credit. Upload it in Nexus's **Header**
field. It is 2064 x 762, about 2.2 MiB; use the header crop to fit the recommended
1300 x 372 display area, keeping the title and credit visible.

[Generation prompt and provenance](header-prompt.md).

## Credits

- CSS by `_eins0fx`.
- [CNS - Custom Nanosuit System](https://www.nexusmods.com/stellarblade/mods/1496?tab=description)
  by DekitaRPG: feature and interaction inspiration.
- [More Beautiful Genessa](https://www.nexusmods.com/mortalshell2/mods/161?tab=description)
  by dantemk2, named BeauteGenessa in the wardrobe.
- [BeauteKnightLady original source](https://www.nexusmods.com/mortalshell2/mods/84?tab=files)
  by dantemk2, visible in the wardrobe list.
- HIT2 DE Scyther by XTGMods, visible in the wardrobe list.
- Seductress development adaptation by `_eins0fx`, based on Skirbie's BG3
  Seductress outfit, with a BeauteGenessa head and rig reference by dantemk2.
  Source body assets credit Larian and Volno's Lazy Tailor library.

The outfits are separate from CSS. These screenshots do not grant permission
to redistribute their source meshes or textures.

## Image links

All PNGs preserve the original 2560 x 1440 screenshots without retouching.

| File | Contents |
| --- | --- |
| [seductress-inventory-polearm.png](v0.1.1/seductress-inventory-polearm.png) | Lead image and video poster, inventory preview with polearm |
| [seductress-inventory-axatana.png](v0.1.1/seductress-inventory-axatana.png) | Inventory preview with Axatana selected; screenshot notification visible |
| [seductress-gameplay-front.png](v0.1.1/seductress-gameplay-front.png) | Front view with axe during gameplay |
| [seductress-gameplay-side.png](v0.1.1/seductress-gameplay-side.png) | Close side view during gameplay |
| [wardrobe-genessa.png](v0.1.1/wardrobe-genessa.png) | BeauteGenessa and the appearance browser |
| [colors-crimson.png](v0.1.1/colors-crimson.png) | Seductress, Crimson palette and RGB controls |
| [wardrobe-seductress.png](v0.1.1/wardrobe-seductress.png) | Seductress in the appearance browser |
| [colors-verdigris.png](v0.1.1/colors-verdigris.png) | Seductress, Verdigris palette controls |
| [seductress-gameplay.png](v0.1.1/seductress-gameplay.png) | Seductress in the game world |

Raw URL prefix:

```text
https://raw.githubusercontent.com/ngodn/CustomShellSystem/main/docs/media/v0.1.1/
```

Keep these filenames stable. Put future captures in a new version folder so
existing page links keep showing the intended footage.

## Video

[css-wardrobe-demo.mp4](v0.1.1/css-wardrobe-demo.mp4) is an H.264/AAC MP4 at
2560 x 1440 and 60 fps, with the original duration and AAC audio retained.
Its MP4 header is placed first for progressive playback. The original recording
remains in local publishing references.

The web copy uses FFmpeg with libx264, the slow preset, CRF 22, a 10 Mbit/s
maximum video rate and a 20 Mbit buffer. Audio is copied without re-encoding.
Frame timestamps pass through without dropping or duplicating frames. This is
lossy compression, with no resolution or frame-rate reduction.

Encoding options:

```sh
ffmpeg -i recording.mp4 -map 0:v:0 -map '0:a:0?' \
  -c:v libx264 -preset slow -crf 22 -maxrate 10M -bufsize 20M \
  -pix_fmt yuv420p -fps_mode passthrough -c:a copy \
  -map_metadata -1 -movflags +faststart css-wardrobe-demo.mp4
```

The original is about 210 MiB. A smaller web copy avoids
[GitHub's 100 MiB limit for individual Git files](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github).

The README uses a clickable poster linking to the MP4. It does not rely on
GitHub rendering an arbitrary HTML video element in a repository README.

The web file is 74.68 MiB (78,305,108 bytes), down from 209.61 MiB. Both
files decode to 3,791 video frames at the original timing. The source container
reports six additional frames that its decoder does not output. The copied
AAC audio has the same stream hash, and the container duration remains 63.3
seconds. [manifest.json](v0.1.1/manifest.json) records the file hashes and probe
results.

## Nexus BBCode

[nexus-gallery.bbcode.txt](nexus-gallery.bbcode.txt) contains ready-to-paste
image tags and a clickable video poster. Paste into the description's source
editor. Raw image URLs go inside `[img]` tags. A raw MP4 goes in a `[url]` link,
not an image tag.

Inline playback of a raw GitHub MP4 in Nexus descriptions has not been verified.
The supplied poster opens the video separately. For an inline Nexus player,
use the editor's supported video provider flow after hosting the recording on
that provider. Do not paste an MP4 URL into a YouTube-ID tag.

See the [Nexus description formatting discussion](https://forums.nexusmods.com/topic/134116-formating-in-descriptions/)
for image, URL and YouTube tag examples. It is historical guidance, so check
the current editor preview before saving.
