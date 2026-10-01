# MISC: the worn shell's own items

The MISC tab's Visibility group has four category rows (Seals, Sidearms, Weapons,
Accessories & Shell Tools). Since 1.0.0-beta.5 it also lists the items the shell you
are wearing carries on its body, under a "<Shell>'s items" heading: Gragu's Revered
Heart, Eredrim's Diapason, Genessa's Catalyst, Proxima's Hook, Thorn's Thorns, Tiel's
Dagger. Each row has its own rule: Follow Accessories (the default), Only When In Use,
Always Hidden, and Always Shown, which keeps that one item while the Accessories rule
hides the rest. The rule is saved as `item:<class>` in `misc_rules`, next to the
category rules, and profiles carry it.

## Why the heart was missing

A Nexus user asked for Gragu's heart to be hideable. The heart is a separate actor,
`WP_AlienHeart`, spawned by the shell's item definition exactly like Eredrim's Diapason,
but it sits in weapon slot `Weapon.Slot.Charges` (it is the shell's charge item), a slot
MISC did not know. It fell through to the socket guess and landed under Weapons, where
nobody would look for it. Charges and Shell.* slots now count as shell items.

## Where the list comes from

MISC already enumerates the mesh's attached item actors on layout changes and once a
second; that pass now runs even when no rule is set, so the rows appear before you have
hidden anything (measured at 0.06 ms a frame on average). Names and one-line
descriptions for the items the game ships live in `misc_shell_item_info()`; any other
item shows its class name made readable. The survey of what each shell spawns is in
`work/research/shells/spawnables.json` (six shells spawn items; Harros, Smert, Solomon,
Necrophage, Lazlo and the Harbinger forms spawn none).

Verified live on Genessa: the Catalyst shows on Always Shown, hides on Always Hidden,
and follows the Accessories rule on the default.

## Gragu's helmet (beta.7)

`BP_Gragu_Helmet` is a separate actor attached to the `Head` bone by a bare scene
root, so the socket rule (body bones are never gear) and the mesh-only child
filter both skipped it. MISC now lets that one non-mesh root through, matched by
its owner class, and lists it under Gragu's items as **Helmet**; it also follows
Accessories & Shell Tools. The root is hidden along with the meshes, so the
per-frame check reads it back as hidden.

The game leaks it: every Player Menu build spawns a helmet for the preview
character and never destroys it when that character goes, leaving it ownerless
and unattached on the preview stage (14 found after one session). Under Gragu's
own head it is invisible; under any other look it shows as a helmet stuck in
every Player Menu tab, which no MISC rule could reach because it is no longer on
the character. `sweep_orphan_helmets` (misc_visibility.inl) destroys helmets
with no owner and no attach parent whenever the menu character changes. The
worn and menu helmets had both in the earlier live check. The October 2 review
tightened cleanup to require no live owner, protecting helmets belonging to
other actors too. It runs when the display character changes and once on menu
close, rather than on every periodic visibility refresh. The non-mesh exception
matches `BP_Gragu_Helmet_C` exactly. These tightened rules still need a live check.
