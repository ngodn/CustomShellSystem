# Color convention for CSS packages

Shipped in **CSS 0.4.0**. This is the standard every CSS outfit package should
follow so that colors look and behave the same across every port, and so a player who has
learned one wardrobe has learned all of them.

[colors.md](colors.md) stays the reference for *how* the color system works: masks,
surfaces, bindings, render targets. This document is about *what a package should declare*
and why. Read that one for the mechanism, this one before authoring or porting.

## Why this exists

The ports were built one at a time and drifted. A survey of every package installed on the
development machine:

| package | controls | palettes |
| --- | --- | --- |
| Seductress V2.1 | `silk, trim, cloth, metal, skin, hair, glow, glow-strength, eyes, eye-glow` | 4 |
| Seductress V2.0.2, V1 | `cloth, metal, skin, face, eyes, eye-glow` | 4 |
| HIT2 DE Scyther | `clothing, metal, ribbons, gems, skin, face, hair` | 2 |
| Beaute Genessa | `clothing, metal, skin, face, eye-glow, eye-intensity` | 2 |
| Beaute KnightLady | `armor, metal, cloth` | 2 |
| BHProxima, CurvyAndCute, LongHairProxima, MileenaOverTiel | none | none |

Three problems, all of them avoidable:

- The same thing is called `cloth`, `clothing`, `armor` and `silk` in four packages, and
  the same glow idea is `eye-glow`, `eye-intensity` and `glow-strength`.
- Most packages ship only two palettes, named for the color rather than the look
  (`crimson`, `midnight`), and nothing tells the player which parts move together.
- Four packages have no color support at all, so the whole tab is dead for them.

## How a final color is produced

Three layers, each one on top of the last:

```
final  =  group tint ( player override  or  palette value )
```

- **Palette** sets every control. It is always the base.
- **Player override** replaces one control's color. This is a custom color.
- **Group tint** is a hue, saturation and brightness adjustment applied to a whole group.

The rules that keep this predictable:

1. Choosing a palette takes over the parts it sets and the tint of the groups those
   parts are in, and leaves everything else alone. Changing the dress does not undo a
   custom skin. `original` is the exception: it means no dye at all, so nothing
   survives it.
2. *Reset part* drops one override. *Reset all* returns to the bare palette.
3. Group tint applies to overrides too, so the pipeline above is the entire story.
4. A control marked `hue_locked` ignores the group's **hue** but still takes its saturation
   and brightness.
5. Group tint needs a palette. On `original` CSS applies no dye and never learns what the
   author's colors are, so there is nothing to shift; the tint rows are hidden there.

Rule 4 is what makes group tint usable. Metal, skin and gems are hue-locked by default, so
dragging the Outfit hue from crimson to teal recolors the fabric and leaves gold as gold
and skin as skin. Without it a group hue shift produces green "gold" and blue skin.

## What a control declares

```json
{
  "id": "garment",
  "name": "Garment",
  "group": "outfit",
  "role": "garment",
  "kind": "color",
  "hue_locked": false,
  "default": [0.56, 0.14, 0.24, 1]
}
```

| field | values | notes |
| --- | --- | --- |
| `id` | stable string | never shown to the player; keep it stable across versions |
| `name` | `"Garment"` | shown in the menu. Sentence case, a noun a player recognises |
| `group` | `outfit` \| `body` | chooses the section and which tint row drives it |
| `role` | see below | the shared vocabulary; drives defaults and palette portability |
| `kind` | see below | what the control *is*; everything but `color` is a single number |
| `hue_locked` | bool | defaults from `role`; set it explicitly when the default is wrong |

### Control kinds

Colour is one kind of control, not the only one. A control declares which it is, and the
menu, the saved look and the apply path all follow from that.

| `kind` | edited as | drives | notes |
| --- | --- | --- | --- |
| `color` | three channels | a dye layer | the default when `kind` is absent |
| `intensity` | one number | a named material scalar | a strength, like eye glow |
| `scalar` | one number | a named material scalar | anything else, like gloss or roughness |
| `toggle` | on or off | material sections | needs `sections`, takes no `min`/`max`/`step` |
| `choice` | which option | a texture parameter | needs `options` and a binding, takes no `min`/`max`/`step` |

A `toggle` lists the material sections it shows and hides, and needs no binding because
it drives them directly:

```json
{"id": "hood", "name": "Hood", "kind": "toggle", "role": "piece",
 "default": [1, 0, 0, 1], "sections": [2, 3]}
```

A `choice` picks between textures the package ships. The value is which option, so the
range is the list and nothing else, and the default names one of them:

```json
{"id": "pattern", "name": "Pattern", "kind": "choice", "role": "pattern",
 "default": [1, 0, 0, 1],
 "options": [{"name": "Plain", "texture": "/Game/CSS/<id>/T_Plain.T_Plain"},
             {"name": "Lace",  "texture": "/Game/CSS/<id>/T_Lace.T_Lace"}],
 "bindings": [{"slot": 0, "parameter": "BaseColorMap  non VT"}]}
```

Between two and sixteen options. Each `texture` is a full object path, with the object
name after the dot, and has to be cooked into this package's own container. A choice
writes into a texture parameter, so unlike a toggle it does need a binding.

Packages written before this said `"type": "scalar"` and meant a strength, so that reads
as `intensity`, not as the new generic `scalar`. Nothing published changes meaning.

### Role vocabulary

Use an existing role wherever the part reasonably fits. Adding a role is fine when nothing
fits, but say so in the package notes.

| role | group | `hue_locked` default | typical part |
| --- | --- | --- | --- |
| `garment` | outfit | no | the main fabric or leather |
| `accent` | outfit | no | trim, lining, ribbons, a secondary fabric |
| `leather` | outfit | no | straps and belts, when they are their own part |
| `metal` | outfit | **yes** | jewellery, buckles, plate, filigree |
| `gem` | outfit | **yes** | stones and crystals |
| `glow` | outfit | no | emissive runes and trim |
| `skin` | body | **yes** | bare skin |
| `face` | body | no | a mask, veil, face paint or makeup |
| `hair` | body | no | hair |
| `eyes` | body | no | the iris |
| `eye-glow` | body | no | the emissive part of the eyes; usually `kind: intensity` |
| `nipple` | body | **yes** | the nipple itself |
| `areola` | body | **yes** | the pigmented ring around it |
| `labia` | body | **yes** | the outer and inner lips |
| `vestibule` | body | **yes** | the inner surface between the inner lips |
| `body-hair` | body | no | pubic and body hair, separate from the head |
| `gloss` | outfit | no | a sheen or roughness slider on the outfit |
| `roughness` | outfit | no | surface roughness, when it is its own control |
| `opacity` | outfit | no | how sheer a garment is |
| `piece` | outfit | no | a part of the outfit a toggle shows or hides |
| `pattern` | outfit | no | which of several textures a garment wears |
| `skin-gloss` | body | no | the body's own sheen |

Hue-locking metal, gems and skin by default is deliberate: those three read as a material
rather than as a color, and rotating their hue is what makes a recolor look broken.

### Bare variants

Many shells have variants that show the body, and those parts want their own controls: a
player who can tint skin but not areolae ends up with a mismatch that is more obvious than
having no control at all.

The intimate pigments are hue-locked with skin on purpose. They are a shade of the body,
not a separate color, so a body hue shift that left them behind would look worse than one
that moved them. A player who wants them pinker or darker sets that control directly,
which is the common case anyway. Pubic hair takes `body-hair` and follows the hair, since
it is normally the same color as the head.

Declare these only on the variants that show them. CSS supports a per-variant `colors`
recipe ([colors.md](colors.md)), so a gowned variant should not list a control for a part
nobody can see. The masks are small, so follow the disjoint-mask rule closely: an areola
mask that bleeds into the breast will tint a patch of skin every time it is used.

## What palettes a package ships

- **`original` is reserved and always present.** It is not a palette a package declares:
  it means CSS applies no dye at all and the author's own materials and textures are used
  exactly as shipped. A palette that merely reproduced those colors would be a composite
  approximation of them, which is worse, so `original` stays outside the palette list and
  first in the menu.
- **At least two palettes** beyond `original`.
- Every palette sets **every outfit control**. A palette is a whole look for the dress,
  so it should never leave one strap behind from the palette before it.
- **Body controls are optional in a palette**, and usually left out. Skin, eyes and the
  intimate pigments belong to the shell the player chose, not to the look of the dress,
  and rule 1 keeps whatever they set. A control the palette leaves out is simply not
  dyed, so it shows the author's own colour: well defined, and the same every time.
  A palette that does mean to change the body (a drowned look, an ashen one) should set
  those controls and say so in its name.
- Palette names are for players, not authors: `"Midnight silver"`, not `"midnight2"`.

```json
"palettes": [
  {"id": "crimson-vow", "name": "Crimson vow", "values": { "...": [] }},
  {"id": "midnight-silver", "name": "Midnight silver", "values": { "...": [] }}
]
```

Declaring a palette with the id `original` is refused, for the reason above.

## Packages that predate this

`group`, `role` and `hue_locked` are optional. CSS infers them from the control id when
they are missing, so every package already published keeps working untouched:

| id contains | inferred role | group | hue locked |
| --- | --- | --- | --- |
| `cloth`, `clothing`, `armor`, `silk`, `garment`, `dress` | `garment` | outfit | no |
| `trim`, `ribbon`, `accent`, `lining` | `accent` | outfit | no |
| `metal` | `metal` | outfit | yes |
| `gem`, `jewel`, `crystal` | `gem` | outfit | yes |
| `skin` | `skin` | body | yes |
| `face`, `mask` | `face` | body | no |
| `hair` | `hair` | body | no |
| `eye-glow`, `eye-intensity` | `eye-glow` | body | no |
| `eyes` | `eyes` | body | no |
| `glow`, `glow-strength` | `glow` | outfit | no |
| anything else | `garment` | outfit | no |

Inference is a compatibility measure, not a substitute. A package updated to this
convention declares all three.

## Checking a recipe

`tools/css_colors.py` has two functions, and they answer different questions.

- `validate(colors)` decides whether CSS can load the recipe safely. It stays permissive
  about the convention on purpose, so that every package published before this document
  keeps working. It runs at package and verify time.
- `lint_convention(colors)` decides whether the recipe is a *good* one: roles declared
  and from the shared vocabulary, groups and hue locking consistent with the role, two
  or more palettes, every palette covering the outfit. Call it from the package's own
  builder and fail the build on it, which is what
  `seductressV2-eins0fx-CSS/v202/tools/build_colors_v2.py` does.

## Do not invent controls

A control for something the shell does not have is worse than no control: the player
drags a slider and nothing moves. Seductress V2 ships no `hair` control because the
shell wears a hood and has no hair surface to dye. Declare what the mesh actually has.

The same goes the other way for variants. CSS takes a `colors` recipe per variant, so a
gowned variant should carry the plain recipe and a bare one the recipe with the intimate
controls in it, rather than every variant offering every control.

## Checklist for a port

1. Every recolorable part has a control with a player-facing `name`, a `group` and a
   `role`.
2. `hue_locked` is correct for each: metals, gems and skin locked; fabric and hair free.
3. Two or more palettes, each setting every outfit control. `original` is not one of
   them: CSS provides it, and it already restores the source mod exactly.
4. Masks are disjoint where parts must stay independent, per [colors.md](colors.md).
5. Verified in game: choose each palette, then drag each group tint end to end, and confirm
   nothing turns an impossible color.
