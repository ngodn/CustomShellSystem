# CSS / CSSX 0.3.0 media

The full PNG header masters are used in Nexus BBCode. The `*-1300x372.png` files are separate Header uploads, fitted inside an exact transparent canvas without cropping or stretching. A 3:1 source needs transparent side margins to preserve the complete composition in a 1300:372 canvas.

- `cssx-header.png`: CSSX cracked-shell logo, pale soul fractures and antique gold sigils.
- `cheat-menu-header.png`: Cheat Menu gauntlet and fractured soul artwork; also used by its library card.
- `ui-kit-header.png`: the UI Kit gallery's existing shared-control artwork.

Original generated PNGs remain in the source repository under assets or the respective extension's assets folder. No game or extension state belongs in this directory.

## Art prompts and method

Built-in image generation/editing was used. CSSX logo edits preserve the user-selected circular shell emblem, horizontal CSSX lettering and a single-line subtitle. The rejected two-line subtitle is not used. Final alignment instructions: subtitle within the CSSX wordmark width, centered divider and `by _eins0fx`, slightly stronger ivory strokes, no reflow or emblem redesign.

Header prompt: preserve that logo on dark charcoal stone with restrained gold cyber-sigils and spectral cracks, clean ivory-gold lettering, full artwork within a fine border. Cheat card prompt: black gauntlet holding a fractured soul sphere, circular antique mechanism, restrained gold on charcoal, no text or human figures. The UI Kit artwork was created earlier for its component gallery.

The user's requested canvas-fitting step uses ImageMagick: resize to fit 1300x372, center on a transparent canvas of exactly that size, retain the full master. See [ImageMagick's extent reference](https://imagemagick.org/command-line-options/#extent).

Release screenshots must show the final 0.3.0 UI, no video. Do not substitute development captures showing earlier versions.

## Screenshots

Captured from the running Steam hotfix build with extension version 0.3.0 shown.
The capture is the game's actual 1324x912 viewport, not an enlarged render.

- `library.jpg`: original 3 by 3 layout, logo fitted into the header only.
- `ui-kit-controls.jpg`, `ui-kit-feedback.jpg`, `ui-kit-search.jpg`,
  `ui-kit-confirmation.jpg`: shared controls and dialogs.
- `cheat-menu-settings.jpg`, `cheat-menu-cheats.jpg`,
  `cheat-menu-resources.jpg`, `cheat-menu-tarstones.jpg`,
  `cheat-menu-shortcuts.jpg`: native extension sections.

All screenshots and header uploads are below Nexus's 8 MB per-image limit.
