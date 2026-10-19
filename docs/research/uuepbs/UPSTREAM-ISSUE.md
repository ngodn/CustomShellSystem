# Proposed upstream change for XTGMods/UUEPBS: accept "brust" in the Breasts group

Game: Mortal Shell II with Custom Shell System outfits (eins0fx). The shared CSS skeleton `SKEL_Base` names its chest helper bones `brust001` and `brust002` (German for breast; leaf bones under `spine_04`, each carrying the breast skin weights). The Breasts group patterns (`boob, breast, bust1, bust, pectoral, breastbase, chestbreast, *breast, breast*, boob*, bust*, *oppai*, *rufang*, *breast*`) do not match, so the Simplified Panel shows no Breasts slider on these bodies while Butt (`butt001/002`), Belly (`belly`), Thighs (`thigh_l/r`), Rib cage (`spine_02`), Waist (`spine_01`), Hips (`pelvis`) and Shoulders (`clavicle_l/r`) all bind.

Change: add `brust`, `brust#` and `*brust*` to the Breasts group's `Bones` list (patch `breasts-brust.patch`, generated from the v2.6.1 dictionary at commit 7c54929). Other German-named rigs benefit too.

Workaround we ship meanwhile, for players, in `Scripts/GameProfiles/MortalShell2.json`:

```json
"BodyGroups": { "Breasts": ["brust001", "brust002"] }
```
