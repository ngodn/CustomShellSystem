### Fixed

- The colour swatch strip was invisible. `flat_button()` clears every brush a CSS button has, so setting a background colour on one paints nothing; 0.4.0 put twelve transparent squares on the page. A chip is now a box with a transparent button on it to take the click, the way the list rows have always drawn theirs.
- Twelve 84px chips in four columns ran the grid down to y=786, where the direction hint landed on top of the Exact color button. Chips are 56px in six columns, so twelve fit in two rows.
- Scrolling onto a colour part could make the CSS and CSSX tabs disappear from the inventory. `inventory_children(canvas)` walks every widget on the page to build the slide-in and took the default limit of 64; the swatch grid pushed the page over it, and the guard threw from inside the panel build, so CSS reported itself unavailable. The limit is now 256, matching the extension page, and that call is wrapped so a failure costs the slide-in animation and nothing else.
- A part the selected palette does not set no longer claims that palette's name. Palettes dress the outfit and leave the body alone, so Skin, Mask and Eyes read "Midnight silver" while showing the author's own colour. They read "Original" now.

### Upgrading

Close the game and extract `MSII-CSS-v0.4.1.zip` into `MortalShell2/Binaries/Win64/ue4ss/Mods/`, replacing the CSS runtime files. Keep `CustomShellSystem/state/`, CSSX, extensions and your installed outfit packages.

**Only CSS needs updating, and nothing else changes from 0.4.0. Existing CSS outfit and port ZIPs keep working. CSSX, UI Kit and Cheat Menu remain at 0.3.0.**
