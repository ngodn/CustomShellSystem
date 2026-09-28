### CSS 1.0.0-beta.5-hotfix.2

### Fixed

- Outfit packages with long dye mask names load on a default Steam install (`C:\Program Files (x86)\Steam\...`). The package cache moved from `cache/packages/<id>/<64-hex manifest hash>/` to `cache/packages/<16-hex manifest hash>/`. The old layout pushed the mask temp files of Curvy and Cute (264 characters), BTGG (265) and Beaute Knight Proxima (263) past Windows' 259-character limit, and the failed write rejected the whole package. The longest possible cached path is now 242 characters.
- Old-layout cache folders are removed on the next scan.
