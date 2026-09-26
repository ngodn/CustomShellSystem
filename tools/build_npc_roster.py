"""Build catalog/npc-appearances.css.json: every enemy, NPC and Harbinger mesh on the
player's skeleton (from work/research/skel/skeletons.jsonl), with the names the game or
the community uses. Dismemberment, LOD, test and clone meshes are left out."""
import json, re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
recs = [json.loads(l) for l in open(ROOT / 'work/research/skel/skeletons.jsonl')]
human = {r['path'] for r in recs if r['kind'] == 'mesh' and r['skeleton'] and 'SKEL_Human_Skeleton' in r['skeleton']}
def game_path(p):
    p = re.sub(r'^MortalShell2/Content/', '/Game/', p).replace('.uasset', '')
    return f"{p}.{p.rsplit('/', 1)[1]}"
# folder-relative mesh name -> (group, display name)
ROSTER = {
 # ---- Enemies (community names; the game does not name ordinary enemies)
 'Enemies/Aristocrat/SK_Aristocrat': ('enemies', 'Aristocrat'),
 'Enemies/BallBoy/SK_BallBoy': ('enemies', 'Ball Boy'),
 'Enemies/BallBoy/SK_BallBoy_02': ('enemies', 'Ball Boy, second look'),
 'Enemies/BallistaHead/SK_BallistaHead': ('enemies', 'Ballista Head'),
 'Enemies/BallistaHead/SK_BallistaHead_Armored': ('enemies', 'Armored Ballista Head'),
 'Enemies/BallistaHead/SK_BallistaHead_Miniboss': ('enemies', 'Ballista Head, miniboss'),
 'Enemies/Batushka/SK_Batushka': ('enemies', 'Viletongue Batushka'),
 'Enemies/Brigands/BrigBase/MatFix/SK_BrigBase': ('enemies', 'Brigand'),
 'Enemies/Brigands/BrigBase/Regal/SK_BrigHorde_Regal': ('enemies', 'Regal Brigand'),
 'Enemies/Brigands/BrigElite/SK_BrigElite': ('enemies', 'Brigand Elite'),
 'Enemies/Brigands/BrigElite/SK_BrigElite_02': ('enemies', 'Brigand Elite, second look'),
 'Enemies/Brigands/BrigElite/SK_BrigElite_Boss': ('enemies', 'Brigand Elite, miniboss'),
 'Enemies/Brigands/BrigKnightly/Art/SK_BrigKinghtly': ('enemies', 'Knightly Brigand'),
 'Enemies/Brigands/BrigRanged_Armored/SK_BrigRanged_Armored': ('enemies', 'Armored Ranged Brigand'),
 'Enemies/Brigands/BrigRanged_Rock/SK_BrigRanged_Rock': ('enemies', 'Ranged Brigand'),
 'Enemies/Brigands/BrigTanky/SK_BrigTanky': ('enemies', 'Heavy Brigand'),
 'Enemies/Brigands/BrigTanky/SK_BrigTanky_NoHelmet': ('enemies', 'Heavy Brigand, no helmet'),
 'Enemies/Brigands/BrigTanky/SK_BrigTanky_Rusty': ('enemies', 'Rusty Heavy Brigand'),
 'Enemies/Brigands/Brigand_Robe/SK_Brigand_Robe': ('enemies', 'Robed Brigand'),
 'Enemies/Brigands/Heavy/SK_Brigand_Heavy': ('enemies', 'Brigand Brute'),
 'Enemies/Brigands/HordeBrigand/SK_Brigand_Depraved': ('enemies', 'Depraved Brigand'),
 'Enemies/Brigands/HordeBrigand/Art/Mesh_02/SK_Depraved_Body_Naked': ('enemies', 'Depraved, bare'),
 'Enemies/Brigands/HordeBrigand/Art/TarBody/SK_BrigHorde_TarBody': ('enemies', 'Tarred Horde Brigand'),
 'Enemies/Brigands/HordeShieldBrigand/SK_Brigand_Horde_Cage': ('enemies', 'Caged Horde Brigand'),
 'Enemies/Brigands/Medium/SK_BrigMed_Baghead': ('enemies', 'Baghead'),
 'Enemies/Brigands/Medium/SK_MS1_Brigan_V1': ('enemies', 'Fallgrim Brigand'),
 'Enemies/Brigands/Medium/SK_MS1_Brigand_V3': ('enemies', 'Fallgrim Brigand, second look'),
 'Enemies/Brigands/Medium/SK_MS1_Brigand_Ranged': ('enemies', 'Fallgrim Ranged Brigand'),
 'Enemies/CannibalKnight/SK_CannibalKnight': ('enemies', 'Cannibal Knight'),
 'Enemies/CannibalKnight/Art/MiniBoss/Mesh/SK_CannibalBoss': ('enemies', 'Cannibal Knight, miniboss'),
 'Enemies/CentipedeGhost/SK_CentipedeGhost': ('enemies', 'Centipede Ghost'),
 'Enemies/CultistBase/MatFix/Base/SK_CultistBase': ('enemies', 'Cultist'),
 'Enemies/CultistSpearLady/SK_CultistSpearLady': ('enemies', 'Cultist Spear Lady'),
 'Enemies/CultistSpearLady/SK_CultistSpearLady_Cloth2': ('enemies', 'Cultist Spear Lady, second robe'),
 'Enemies/MS1_HeavyCultist/SK_HeavyCultist': ('enemies', 'Heavy Cultist'),
 'Enemies/Draugr/SK_Draugr': ('enemies', 'Draugr'),
 'Enemies/DungeonChampion/SK_DungeonChampion': ('enemies', 'Dungeon Champion'),
 'Enemies/FrogMama/SK_FrogMama': ('enemies', 'Frog Mama'),
 'Enemies/GrishaHunter/SK_GrishaHunter': ('enemies', 'Grisha Hunter'),
 'Enemies/HutchbackCarrier/SK_HutchbackCarrier': ('enemies', 'Hunchback Carrier'),
 'Enemies/MS1_TwinSisters/SK_TwinSisters': ('enemies', 'The Silent Sester'),
 'Enemies/MS1_TwinSisters/SK_TwinSisters_02': ('enemies', 'The Silent Sester, twin'),
 'Enemies/Miner/SK_Miner': ('enemies', 'Resurrected Miner'),
 'Enemies/Sicario/SK_Sicario': ('enemies', 'Sicario'),
 'Enemies/TarredCorpse/SK_TarredCorpse': ('enemies', 'Tarred Corpse'),
 'Enemies/TarredStoner/SK_TarredStoner': ('enemies', 'Tarred Stoner'),
 'Enemies/Warden/SK_Warden': ('enemies', 'Warden'),
 'Enemies/Warden/SK_Warden_Maskless': ('enemies', 'Warden, no mask'),
 'Enemies/Wraith/SK_Wraith': ('enemies', 'Gloombound Wraith'),
 'MS1/Characters/Enemies/Fallgrim/Vampire/Mesh/SK_MS1_Vampire': ('enemies', 'Fallgrim Vampire'),
 'Bosses/Offspring/SK_Offspring': ('enemies', 'Offspring'),
 # ---- People (names from ST_Core_NpcNames)
 'NPCs/Blacksmith/Blacksmith_Old/SK_Blacksmith': ('people', 'Franz'),
 'NPCs/SesterGenessa/Sk_Sester_Genessa': ('people', 'Sester Genessa, first form'),
 'NPCs/SesterGenessa/Sk_Sester_Genessa_V4_Corrupted': ('people', 'Corrupted Sester'),
 'NPCs/SesterGenessa/SK_Sester_Genessa_V6_RedGhost': ('people', 'Sester Genessa, red ghost'),
 'NPCs/VillageCultist/SK_VillageCultist_Female': ('people', 'Villager, woman'),
 'NPCs/VillageCultist/SK_VillageCultist_Male': ('people', 'Villager, man'),
 'NPCs/VillageCultist/SK_VillageCultist_MaleChild_low': ('people', 'Villager, child'),
 'NPCs/Vlas/SK_PoorVlas': ('people', 'Vlas'),
 'NPCs/William/SK_Willam': ('people', 'William'),
 'Enemies/FrogMama/SK_Hilga': ('people', 'Hilga'),
 # ---- Harbinger forms (names from ST_Core_DarkForms)
 'Shells/DarkForm/SK_DarkForm': ('harbinger', 'Harbinger'),
 'Shells/DarkForm/SK_DarkForm_02': ('harbinger', 'Harbinger, second look'),
 'Shells/DarkForm_Bulk/SK_DarkForm_Bulk': ('harbinger', 'Harbinger, bulk'),
 'Shells/DarkForm_Bulk/SK_DarkForm_Bulk_FrogMask': ('harbinger', "Harbinger with Hilga's Mask"),
 'Shells/DarkForm_Bulk/SK_DarkForm_Bulk_Robe': ('harbinger', 'Robed Harbinger'),
 'Shells/DarkForm_Bulk/SK_DarkForm_Bulk_OnlyRobe': ('harbinger', 'Harbinger, robe only'),
 'Shells/DarkForm_Bulk/SK_DarkForm_Bulk_backpack': ('harbinger', 'Harbinger with pack'),
 'Shells/DarkForm_Skel/SK_DarkBro_SKeleton': ('harbinger', 'Skeletal Harbinger'),
}
GROUPS = {'enemies': ('Enemies', 'Wear an enemy the way the game draws it. Your shell keeps its abilities; the enemy keeps its default materials.'),
          'people': ('People', "Wear one of the game's people. Your shell keeps its abilities."),
          'harbinger': ('Harbinger forms', 'Wear a Harbinger form while you still have a shell.')}
outfits = {g: {'id': f'css.npc.{g}', 'name': name, 'author': 'Cold Symmetry', 'description': desc,
               'compatibility': 'same_skeleton', 'shells': ['CharacterId.Player.Shell.Genessa'], 'variants': []}
           for g, (name, desc) in GROUPS.items()}
by_name = {}
for path in human: by_name.setdefault(path.rsplit('/', 1)[1][:-len('.uasset')].lower(), []).append(path)
missing = []
for rel, (group, name) in ROSTER.items():
    base = rel.rsplit('/', 1)[1].lower()
    found = by_name.get(base, [])
    if len(found) != 1: missing.append((rel, found)); continue
    vid = re.sub(r'[^a-z0-9]+', '_', base).strip('_')
    outfits[group]['variants'].append({'id': vid, 'name': name, 'mesh': game_path(found[0])})
doc = {'schema': 1, 'outfits': list(outfits.values())}
out = ROOT / 'packaging/catalog/npc-appearances.css.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(doc, indent=1) + '\n')
print('variants:', {g: len(o['variants']) for g, o in outfits.items()}, '->', out.relative_to(ROOT))
for m in missing: print('NOT ON HUMAN SKELETON / PATH MISMATCH:', m)
