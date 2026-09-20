# Control convention for CSS packages

The standard every CSS outfit package should follow, so that a wardrobe behaves the same
across every port and a player who has learned one has learned all of them.

Colour shipped in **CSS 0.4.0**. **1.0** widened it: a package declares **controls**, and
colour is one kind of control alongside switches, texture choices, material scalars and
live springs. The tab is called CUSTOMIZE, and the manifest block is `customize`.

> Renamed in 1.0. The block used to be `colors` and this file used to be
> `color-convention.md`. Nothing published breaks: the runtime and the authoring tools read
> `colors` whenever `customize` is absent, and the old request actions `color` and
> `reset_color` still work. Naming both in one place is refused rather than guessed at.

[colors.md](colors.md) stays the reference for *how* the dye system works: masks,
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
| `spring` | two/three numbers | the mesh's own spring bones | Bounce Hz, Settle %, optional Travel cm and explicit axis filters |
| `dynamics` | three numbers | post-process AnimDynamics chain roots | Stiffness, Damping, Gravity; experimental integration |
| `shape` | one number | a morph target on the package's own mesh | needs `morph`, optional joint `formulas` |
| `glow` | radiance & pulse | emissive material parameters | intensity cd/m², pulse Hz, combat reactivity |
| `opacity` | one number (0..1) | alpha / sheerness parameter | sheer fabrics, lace, stockings, chiffon |

A `toggle` lists the material sections it shows and hides, and needs no binding because
it drives them directly:

```json
{"id": "hood", "name": "Hood", "kind": "toggle", "role": "piece",
 "default": [1, 0, 0, 1], "sections": [2, 3]}
```

`sections` and `occludes_sections` contain global mesh material-slot indices, not
render-section indices. UE can split one material into several render sections or
remap section materials at a LOD. Native calls use `MaterialID=slot` and
`SectionIndex=INDEX_NONE` for the player and menu components, including restoration.
This bypasses the section map while preserving the intended material ID. The pinned
UE 5.6.1 implementation is `SkinnedMeshComponent.cpp:3731`; a real component probe
with map `[1, 0]` confirms that the old `(0, 0)` call hides material 1 while
`(0, -1)` hides material 0. SeduXtress V30's four wardrobe states and reset also pass
component read-back. This is editor evidence; fresh game verification is pending.

The development control catalog lives at `tests/fixtures/dev-control-kinds.css.json`.
It is a test fixture and is excluded from the installer's `catalog/*.css.json` glob.

Optional `occludes_sections` lists body or underlayer sections covered while the toggle
is on. For example, footwear section 20 covers body feet 21 and stocking feet 22:

```json
{"id": "boots", "name": "Footwear", "kind": "toggle", "role": "piece",
 "default": [1, 0, 0, 1], "sections": [20], "occludes_sections": [21, 22]}
```

Both arrays contain integer material indices from 0 to 127. An authored array cannot
be empty. Covered indices must be unique and distinct from that toggle's own sections.
Different controls may cover the same section. The runtime combines all hidden
sections with item masks, so removing one garment restores a section only when no
other active mask needs it hidden. A stocking toggle can therefore own section 22
while footwear covers it. Defaults also apply to Original and after clearing saved
overrides. This changes visibility only; it does not reshape or delete body geometry.
Host tests cover these decisions; live acceptance of the new occlusion field is pending.

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

A `shape` drives a morph target, so a slider changes the geometry rather than a material:

```json
{"id": "hips", "name": "Hips", "kind": "shape", "group": "body", "role": "figure",
 "morph": "Hips", "min": 0, "max": 1, "step": 0.05, "default": [0, 0, 0, 1]}
```

**Authored meshes only, and this will never change.** Every stock shell carries zero morph
targets: Sester Genessa V6, Shell KnightLady V04 and Tiel all read back an empty
`MorphTargets` array from the running game. A shape slider can only ever move a mesh CSS
cooked itself, where the shape came in from a Blender shape key. There is no path to a body
slider on an unmodded shell, and a package must not promise one.

The chain is: a Blender shape key, exported by `tools/authoring/export_css_mesh.py` into the
mesh JSON's `morph_targets`, imported by the `CSSImportMesh` commandlet, cooked, and named
here in `morph`. The name has to survive all of that, so it is letters, digits and
underscores only, at most 64 characters, and the exporter and the importer both refuse
anything else rather than letting the engine silently rename it.

`min`, `max`, `step` and `default` work exactly as they do for a scalar. A range that goes
negative is fine if the author cooked the shape to read that way.

**A shape only works on a mesh bound to the game's skeleton.** CSS refuses to wear a mesh
whose `Skeleton` asset differs from the one already worn, and `CSSImportMesh` always
creates an authoring stub. `CSSBindClothV3 -SkeletonOnly -Mesh=... -Skeleton=/Game/Sparta/
Characters/Humans/_Shared/SKEL_Human_Skeleton` is the step that assigns the real one, and
it refuses a mesh whose bones the skeleton does not have. That is the real reason the stub
must never ship: a mesh still bound to it cannot be worn at all, shapes or no shapes.

**Check the morph exists before shipping.** At runtime a control CSS cannot apply takes the
whole outfit off, which is the right call for a half-applied look and a miserable way to
find a typo. `check_shapes(recipe, shape_names(mesh_json))` in `tools/css_controls.py`
compares the recipe against the mesh JSON and needs no editor. `CSSInspectMesh` reads a
saved asset back and reports every morph target with the number of vertices it moves and
its largest delta, which is the check to run after an import.

A `spring` tunes live secondary motion: how a bust, a belly or a hip moves when the
character does. It is the only control with two numbers in it, and the only one that
writes no material at all.

```json
{"id": "bust", "name": "Bust", "kind": "spring", "group": "body", "role": "figure",
 "nodes": ["brust001", "brust002"],
 "frequency": {"min": 1.2, "max": 2.6, "default": 1.5915},
 "damping_ratio": {"min": 0.4, "max": 0.95, "default": 0.65}}
```

`nodes` names the bones, not the blueprint properties. A blueprint names its spring nodes
`AnimGraphNode_SpringBone`, `_1`, `_2` and so on in compile order, and recompiling can
shuffle that; the bone a node drives does not move. Between one and thirty-two bones, each
spelled as the cooked skeleton spells it.

**Frequency and damping ratio, not stiffness and damping.** `FAnimNode_SpringBone`
integrates `a = K*error - D*velocity` at a fixed 1/120 s with no mass term, so the system
is `x'' + D x' + K x = 0`, `K = (2*pi*f)^2` and `D = 4*pi*zeta*f`. CSS does that conversion,
so an author states what the part should do rather than which numbers happen to produce it,
and the player gets a slider in hertz instead of a slider in engine units. The menu calls
them Bounce and Settle.

Both defaults have to be what the animation blueprint already ships, or the menu opens on a
value the body is not at. `spring_defaults(stiffness, damping)` in `tools/css_colors.py`
converts the blueprint's own numbers, so copy what it prints rather than guessing.

Frequency tops out at 8 Hz and damping ratio at 2, and a range whose stiff-and-damped corner
would cross the engine's own damping cutoff is refused: past that the engine scales damping
down instead of using what it was given, and the slider would stop meaning what it says.

A spring is live and reversible. CSS records what the blueprint held the first time it
touches a node and puts that back when the control is dropped or the outfit is removed, so
nothing needs a mesh reload to undo.

`translate` and `rotate` are optional three-boolean arrays. Omitted arrays retain the
captured blueprint flags, including when a travel clamp is present. CSS does not infer
axis settings from names such as `brust`, `butt`, `hip` or `thigh`. Explicit `rotate`
flags can enable or disable the corresponding SpringBone rotation filters.

`planar_constraint` accepts `none`, `x`, `y` or `z`. It locks translation on the named
axis after explicit translation overrides, so `translate: [true,true,true]` cannot
bypass `planar_constraint: "y"`. The other translation and rotation flags retain their
authored or explicitly overridden values. These SpringBone translation filters act
in **world space** in UE 5.6.1; they are not a character-relative anatomical collision
plane. A travel clamp bounds displacement but does not guarantee garment clearance.

`world_damping`, `limit_angle`, `collision_radius` and `gravity_scale` are reserved
metadata from the earlier design. Their presence and finite numeric ranges are checked
by the package and native parsers. Omission is retained separately from an explicit
zero. The current SpringBone runtime adapter does **not** apply them. Do not describe
these accepted fields as active collision, cone limits, gravity or Kawaii Physics.
The separate AnimDynamics experiment and remaining native adapter are tracked in
[the dynamics investigation](../../CSS-Mod-Authoring/docs/next-gen-dynamics-investigation.md).

Packages written before this said `"type": "scalar"` and meant a strength, so that reads
as `intensity`, not as the new generic `scalar`. Nothing published changes meaning.

### AnimDynamics controls (experimental)

The native branch now accepts `kind: "dynamics"` for UE's built-in AnimDynamics
nodes. This path has offline parser, persistence and build checks. It is not yet
accepted for distribution: the reset impulse described in the
[dynamics investigation](../../CSS-Mod-Authoring/docs/next-gen-dynamics-investigation.md)
still needs synchronization with actual animation evaluation and live verification.

```json
{
  "id": "ponytail_dynamics",
  "name": "Ponytail dynamics",
  "kind": "dynamics",
  "group": "body",
  "role": "motion",
  "nodes": ["CSS_Hair_Ponytail_01"],
  "angular_spring": {"min": 0, "max": 200, "default": 80},
  "damping": {"min": 0.7, "max": 1, "default": 0.8},
  "gravity": {"min": -1, "max": 1, "default": 0.1}
}
```

`nodes` names each solver's **root bone**, once, not every member of an articulated
chain. Each root has one control owner. CSS resolves roots on the active post-process
instance and rejects missing or ambiguous roots before applying a control. There are
at most 32 roots per control and 32 discovered solver nodes per instance.

The three saved channels contain direct solver values, in order:

| Channel | Field and supported range | Applied engine settings |
| --- | --- | --- |
| 0 | `angular_spring`, 0..1000 | `AngularSpringConstant`; zero disables `bAngularSpring` |
| 1 | `damping`, 0.7..1 | Both damping overrides, with both override flags enabled |
| 2 | `gravity`, -5..5 | `GravityScale`, with gravity override disabled |
| 3 | Reserved, always 1 | No engine write |

Author ranges may be narrower; defaults must match the cooked graph. The menu uses
steps of 1, 0.01 and 0.05, capped by each range's width. Damping here is a solver
override, not a SpringBone damping ratio; the engine documents a 0.7 floor.
[UE 5.6 AnimDynamics reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AnimNode_AnimDynamics?application_version=5.6).

This kind does not accept material bindings, SpringBone ranges or the reserved
SpringBone extension fields. Authored constraints, collision shapes, simulation
space, component acceleration and angular targets stay in the graph. Runtime
collision tuning is still unfinished.

Profiles and palettes can retain values above 32 and negative gravity. Unbound
snapshots receive broad finite bounds first; installed control schemas then enforce
the actual channel ranges. Existing color/scalar/spring bounds remain unchanged.
An incompatible value is dropped during variant compatibility filtering.

The adapter captures the four numeric settings and four associated flags on first
touch, reads back writes, and restores the captured values on Original reset, removal
or shutdown. It requests `ResetDynamics(ResetPhysics)` when damping values/override
flags or the gravity-override mode change, because those are copied into solver
bodies during initialization. Angular spring settings and gravity scale update
during evaluation and do not require a reset. The reset enum is discovered
from the function's reflected parameter, and its signature is checked before any
node mutation. A successful reflected write/reset request alone does not establish
the solver's evaluated behavior.

### Items and slots

A variant may list `items` instead of a single `mesh`. Exactly one item takes the `body`
slot and replaces the character mesh, which is what every package published before 1.0
does, so those load unchanged as one-item packages. The rest are accessories: their own
skeletal mesh components, attached to the body and posed by it.

```json
"items": [
  {"id": "body", "name": "Body", "slot": "body", "mesh": "/Game/CSS/<id>/SK_Body"},
  {"id": "collar", "name": "Bunny collar", "slot": "neck", "order": 20,
   "mesh": "/Game/CSS/<id>/SK_Collar", "hides": {"sections": [3, 4]}}
]
```

Sixteen slots: `body`, `head`, `hair`, `face`, `ears`, `neck`, `chest`, `back`, `hands`,
`waist`, `legs`, `feet`, and `trinket1` to `trinket4` for pieces with no natural home. One
item per slot: two items claiming the same one is refused rather than silently stacked.
`order` layers them, low first.

`hides.sections` names body material sections the item covers, so a boot can stop a foot
poking through. It shares the bookkeeping a `toggle` uses, so taking the outfit off puts
back exactly what CSS hid and nothing the game hid itself.

**An accessory mesh needs a real skeleton.** It is posed by the body through a leader pose,
so its bones have to be the body's bones. A mesh with a token skeleton will attach, report
itself visible, sit at the right place and render nothing at all.

## Role vocabulary

Use an existing role wherever the part reasonably fits. Adding a role is fine when nothing
fits, but say so in the package notes.

| role | group | `hue_locked` default | typical part |
| --- | --- | --- | --- |
| `garment` | outfit | no | the main fabric or leather |
| `accent` | outfit | no | trim, lining, ribbons, a secondary fabric |
| `leather` | outfit | no | straps and belts, when they are their own part |
| `fabric` | outfit | no | capes, cloaks, scarves, skirts, sashes, sheer lace |
| `headwear` | outfit | no | hats, crowns, tiaras, hairpins, veils |
| `jewelry` | outfit | **yes** | necklaces, chokers, earrings, rings, bracelets |
| `metal` | outfit | **yes** | jewellery, buckles, plate, filigree |
| `gem` | outfit | **yes** | stones and crystals |
| `glow` | outfit | no | emissive runes and trim |
| `skin` | body | **yes** | bare skin |
| `face` | body | no | a mask, veil, face paint or makeup |
| `hair` | body | no | hair, ponytails, bangs, braids |
| `eyes` | body | no | the iris |
| `eye-glow` | body | no | the emissive part of the eyes; usually `kind: intensity` |
| `breast` | body | no | breast & bust shape, volume, or jiggle motion |
| `butt` | body | no | buttocks & glute projection, shape, or jiggle motion |
| `thigh` | body | no | hips, upper thighs, inner thigh tissue |
| `waist` | body | no | waist curve, belly volume |
| `nipple` | body | **yes** | the nipple itself |
| `areola` | body | **yes** | the pigmented ring around it |
| `labia` | body | **yes** | the outer and inner lips |
| `vestibule` | body | **yes** | the inner surface between the inner lips |
| `clitoris` | body | **yes** | the clitoris and hood |
| `orifice` | body | **yes** | vaginal opening/depth, anal opening/depth |
| `body-hair` | body | no | pubic and body hair, separate from the head |
| `gloss` | outfit | no | a sheen or roughness slider on the outfit |
| `roughness` | outfit | no | surface roughness, when it is its own control |
| `opacity` | outfit | no | how sheer a garment is |
| `piece` | outfit | no | a part of the outfit a toggle shows or hides |
| `pattern` | outfit | no | which of several textures a garment wears |
| `skin-gloss` | body | no | the body's own sheen, sweat, or oil |
| `figure` | body | no | a spring on a part of the figure: bust, hips, belly |
| `motion` | body | no | a spring on something else that moves, like hair or a cloak |

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

Declare these only on the variants that show them. CSS supports a per-variant `customize`
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

The same goes the other way for variants. CSS takes a `customize` recipe per variant, so a
gowned variant should carry the plain recipe and a bare one the recipe with the intimate
controls in it, rather than every variant offering every control.

## Checklist for a port

1. The block is called `customize`, and every adjustable part has a control with a
   player-facing `name`, a `kind`, a `group` and a `role`.
2. `hue_locked` is correct for each colour: metals, gems and skin locked; fabric and hair
   free. It means nothing to the other kinds and they leave it out.
3. Two or more palettes, each setting every outfit control. `original` is not one of
   them: CSS provides it, and it already restores the source mod exactly.
4. Masks are disjoint where parts must stay independent, per [colors.md](colors.md).
5. A `toggle` names real material sections, a `choice` names cooked textures inside this
   package's own container, a `spring` names bones the cooked skeleton actually has, and a
   `shape` names a morph target the cooked mesh actually carries.
6. A `spring`'s two defaults match what the animation blueprint ships, read with
   `spring_defaults()` rather than guessed.
7. Verified in game: choose each palette, drag each group tint end to end, flip every
   toggle, and move every slider to both ends. Nothing turns an impossible colour, nothing
   disappears that should not, and nothing keeps moving after you stop.

## Templates vs Profiles (1.0.0-beta Next-Gen)

CSS 2.0 cleanly distinguishes between subsystem **Templates** and character **Profiles**:

### Templates (Subsystem Combinations & Presets)
Templates live inside outfit packages (`outfits[].templates`) and are author-curated presets for specific systems:
- **`combinations`**: Multi-toggle & part combinations (e.g. *"Topless Harness Set"*, *"Battle-Damaged Gown"*, *"Bikini Armor"*).
- **`palettes`**: Material & dye color palettes (e.g. *"Crimson Vow"*, *"Void Obsidian"*).
- **`archetypes`**: Body morph combinations (e.g. *"Voluptuous"*, *"Petite Seductress"*, *"Athletic Amazon"*).
- **`physics`**: Jiggle & secondary dynamics presets (e.g. *"Firm Athletic"*, *"Sensual Bouncy"*, *"Ultra Soft"*).
- **`hair`**: Hair style, ponytail sway, and Kawaii physics presets.
- **`accessories`**: Accessory sets and trinket toggles.
- **`fabrics`**: Sheerness and fabric opacity presets.
- **`anatomy`**: Intimate morphs, breast shape, glute curvature, and orifice depth presets.
- **`glow`**: Rune radiance, breathing pulse rate, and combat surge configs.

In Tab 1 (`CUSTOMIZE`), Row 0 provides the **Template Selector**:
- Shows current active template name and category subtitle.
- Left / Right cycles through all available templates and palettes.
- The right-side panel displays direct preset buttons and a **"Browse templates..."** button opening the Searchable Browse Modal.

### Profiles (Global Character Snapshots)
The 4th Tab is **`PROFILE`** (renamed from `TEMPLATES`):
- Saves a global character snapshot spanning all tabs: worn outfit shell, variant, customized colors, tints, morph shapes, physics tuning, walk style, and toggles.
- Save new profiles with suggested naming or custom names.
- Load profiles with 1 click.
- Overwrite and delete actions are protected by **Native Confirmation Dialog Modals**.

## Secondary Physics & Stellar Blade Jiggle Solvers

Inspired by *Stellar Blade* and *Better Jiggle Mod* (Nexus 1570):
1. **Dynamic Frequency & Damping Integration**:
   - Bounce frequency in Hz (stiffness $K = (2\pi f)^2$)
   - Settle damping ratio (decay $D = 4\pi \zeta f$)
2. **Travel Clamping (`max_displacement`)**:
   - Restricts maximum displacement (cm) via `bLimitDisplacement` and `MaxDisplacement`. Collision clearance still requires evaluation against the actual outfit.
3. **3-Axis Rotational Swing (`bRotateX/Y/Z`)**:
   - Explicit `rotate` flags control the node's rotation filters. Absent flags preserve the animation blueprint's values. The engine skips SpringBone evaluation when all translation axes are disabled, even if rotation flags are enabled.
4. **Planar & Lateral Constraints (`planar_constraint`)**:
   - Locks the named world-space translation axis after explicit translation settings. This does not implement body collision.
5. **Reserved dynamics metadata, not applied by SpringBone**:
   - `world_damping`: 0..1.
   - `limit_angle`: 0..180 degrees.
   - `collision_radius`: 0..100 cm.
   - `gravity_scale`: -5..5.
   - Zero and omission are distinct. Solver-specific mappings and active collision are still under development.

## Next-Gen UI Kit Specification

- **Sliders**:
  - HSL Tint Sliders (Hue -180..180°, Saturation 0..200%, Brightness 0..200%)
  - Discrete RGB Channel Sliders (0..255 per channel in exact color mode)
  - Opacity / Sheerness Slider (0..100%)
  - Secondary Physics Sliders (Bounce Hz, Settle %, Travel cm)
  - Morph Weight Sliders (0..100% or min..max)
  - Glow Radiance & Pulse Sliders (Intensity cd/m², Pulse Hz)
- **Color Swatches**:
  - 24-chip deterministic swatch strip for fast 1-click color palette application.
- **Searchable Browse Modal (`native_picker_`)**:
  - Fullscreen/modal searchable list with live text query filtering, mouse-wheel scrolling, keyboard/gamepad navigation, and apply/cancel routing.
  - Used for `ui_browse_shells` (all installed outfits & authors), `ui_browse_templates` (all combinations, palettes, archetypes, and physics presets), and `ui_browse_choice` (multi-texture choices).
- **Confirmation Modals (`confirm_action_`)**:
  - Darkened backdrop modal dialog for destructive actions:
    - Reset all customizations confirmation
    - Delete profile confirmation
    - Overwrite profile confirmation
  - Keyboard/gamepad focus trap (Accept / Cancel) with camera motion suspended.


## CSS ControlRig hair inputs (candidate, 2026-09-20)

`kind: "rig"` addresses the outfit's post-process AnimBP inputs. It is separate from SpringBone frequency/ratio controls and AnimDynamics angular/damping controls. `solver` defaults to `"positional_hair"` for existing packages. One hair control may coexist with independent angular-body controls described below. Rig controls accept no node names or arbitrary property bindings.

```json
{
  "id": "hair_motion", "name": "Hair motion", "kind": "rig", "group": "body", "role": "motion",
  "stiffness": {"min": 100, "max": 250, "default": 150},
  "damping": {"min": 12, "max": 24, "default": 18},
  "gravity": {"min": 0, "max": 0.2, "default": 0},
  "enabled": true
}
```

Saved values use `[stiffness, damping, gravityScale, enabled]`; enabled must be numeric zero or one. Schema limits are stiffness 1..1000, damping 0..120 and gravity scale -5..5. Authors must choose and verify narrower ranges for their mesh. These schema bounds do not certify visual behavior at every combination. Stiffness and damping are the solver's direct positional-force and exponential-velocity-decay coefficients. Gravity scale maps to world acceleration `(0,0,-980*scale)` in cm/s². A zero default preserves the measured rig's authored rest-following response.

The native adapter requires a real AnimInstance under `/Game/CSS/`, float `CSSStiffness` and `CSSDamping`, native double-precision FVector `CSSGravity`, bool `CSSEnabled`, and int32 `CSSResetEpoch`. It validates reflected names, scalar counts, exact property types and storage bounds before writing. It never touches a class default/archetype or scans objects each frame. Settings are captured once on the owning weak instance, restored when the control is removed, and reapplied after an instance replacement. Menu preview synchronization resolves and writes only when settings or the preview instance change.

The AnimBP forwards the first four inputs to the saved ControlRig. Its native animation update advances `CSSUpdateSerial`; repeated evaluation with the same serial reuses output. `CSSResetEpoch` reseeds on the next enabled evaluation. Disable passes through, and re-enable seeds the current incoming pose. Calling engine `ResetDynamics` alone does not reseed this rig; use the CSS epoch bridge. Tuning changes preserve the running clock and take effect on the next evaluation.

Validation: 283 portable control behavior checks, 54 Python package/control tests, and the Windows core build pass. The saved full-hair tuning AnimBP passes 216 component checks; its default output exactly matches the previous V10 wrapper at sampled positions. Native reflection writes, preview restoration, in-game UI, profile reapplication after travel/death and game performance still require live verification with the new candidate. CSSX is not required and remains disabled.

## CSS ControlRig body inputs (candidate, 2026-09-20)

`solver: "angular_body"` selects independent region tuning on the same post-process AnimBP. Each control owns one or more distinct regions. Two body controls cannot own the same region, and a legacy SpringBone or AnimDynamics control cannot also drive that region.

```json
{
  "id": "chest_motion", "name": "Chest motion", "kind": "rig",
  "solver": "angular_body", "group": "body", "role": "figure",
  "regions": ["brust001", "brust002"],
  "frequency": {"min": 0.5, "max": 6, "default": 2},
  "damping_ratio": {"min": 0.1, "max": 2, "default": 0.7},
  "motion_amount": {"min": 0, "max": 1, "default": 1},
  "enabled": true
}
```

Saved values are `[frequencyHz, dampingRatio, motionAmount, enabled]`, with numeric zero or one for enabled. Schema bounds are 0.5..6 Hz, 0.1..2 damping ratio and 0..1 motion amount. Slider steps are 0.1, 0.05 and 0.05, bounded by the declared range width. These bounds are validation limits, not acceptance of all combinations on every outfit. Body controls reject hair `stiffness`, `damping` and `gravity` fields. Hair controls reject body fields. Unknown solvers are rejected.

Canonical region order is `brust001`, `brust002`, `butt001`, `butt002`, `thigh_twist_02_l`, `thigh_twist_02_r`, `belly`. The AnimBP must expose seven-entry float arrays `CSSBodyRegionFrequencies`, `CSSBodyRegionDampingRatios`, `CSSBodyRegionMotionAmounts`, a seven-entry bool array `CSSBodyRegionEnabled`, bool `CSSBodyUseRegionSettings`, float globals `CSSBodyFrequency`, `CSSBodyDampingRatio`, `CSSBodyMotionAmount`, and int32 `CSSBodyResetEpoch`.

The native adapter validates the real CSS AnimInstance, reflected types, bounds, array storage and finite values before writing. It captures player and preview defaults separately. Active body controls are composed from the captured player defaults, preserving untargeted regions. If authored region mode is off, global values initialize all seven regions before overrides enable region mode. Removing controls restores captured inputs; replacing an instance invalidates captured state. Preview synchronization writes the owned arrays and switch, leaving its global defaults intact for later restoration.

Changed body inputs advance only `CSSBodyResetEpoch`. Hair retains `CSSResetEpoch`. General dynamics reset advances both, while unchanged body input values perform no write or reset. No body solver or object scan is added to a per-frame native hook. The UI uses Frequency (Hz), Damping ratio, Motion amount and Motion on/off. Profiles use the existing four-channel saved-control representation.

Current evidence: Windows core compilation, 307 portable control checks and 16 metadata tests pass. The actual combined editor component passes 23 fixtures and 37542 checks, including independent reset isolation. This does not verify the UE4SS reflection adapter in the game. Native UI interaction, preview/profile recovery, cooked execution and live performance remain required. See [the body-region checkpoint](../../CSS-Mod-Authoring/docs/next-gen-body-region-controls.md).
