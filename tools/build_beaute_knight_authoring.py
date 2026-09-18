#!/usr/bin/env python3
"""Build authoring assets and color recipes for CSS_BeauteKnightPROXIMA_dantemk2."""
import hashlib
import json
import shutil
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
SCRATCH = Path("/home/eins0fx/.gemini/antigravity-cli/brain/5d48e097-d29c-4116-a195-17848c60af29/scratch/bkn_audit")
THUMBNAIL_SRC = Path("/home/eins0fx/development/mods/msII/CustomShellSystem/work/release-v0.3.2/packages/thumbnails/beaute-knightlady.png")

AUTHORING = ROOT / "authoring"
AUTHORING.mkdir(parents=True, exist_ok=True)

# Copy thumbnail
shutil.copy2(THUMBNAIL_SRC, AUTHORING / "thumbnail.png")

# Materials mapping
materials_map = {
    "0": "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_1004.MI_LadyKnight_1004",
    "1": "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_1001.MI_LadyKnight_1001",
    "2": "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_1002.MI_LadyKnight_1002",
    "3": "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_1003.MI_LadyKnight_1003",
    "4": "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_Proxima_HandSpear_01.MI_Proxima_HandSpear_01",
    "5": "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_Proxima_HandSpear_01.MI_Proxima_HandSpear_01",
    "6": "/Game/Sparta/Characters/Shells/Smert/Mesh/Textures/MI_Smert_HoodRobe_01.MI_Smert_HoodRobe_01",
}
(AUTHORING / "materials.json").write_text(json.dumps(materials_map, indent=2) + "\n")

def smooth(x, lo, hi):
    t = np.clip((x - lo) / (hi - lo), 0, 1)
    return t * t * (3 - 2 * t)

def layer(path: Path, rgb: np.ndarray, mask: np.ndarray, gain: float = 1.0):
    linear = np.where(rgb <= 0.04045, rgb / 12.92, ((rgb + 0.055) / 1.055) ** 2.4)
    light = np.clip(np.max(linear, axis=2) * gain, 0, 1)
    gray = np.where(light <= 0.0031308, 12.92 * light, 1.055 * (light ** (1 / 2.4)) - 0.055)
    out = np.concatenate((np.repeat(gray[..., None], 3, axis=2), np.clip(mask, 0, 1)[..., None]), axis=2)
    Image.fromarray(np.rint(out * 255).astype(np.uint8)).save(path, optimize=False)

def write_dye_layer(target_dir: Path, rgb: np.ndarray, mask: np.ndarray, gain: float) -> str:
    temp_path = target_dir / "_temp_dye.png"
    layer(temp_path, rgb, mask, gain)
    digest = hashlib.sha256(temp_path.read_bytes()).hexdigest()[:32]
    filename = f"dye-{digest}.png"
    dest_path = target_dir / filename
    temp_path.replace(dest_path)
    return filename

# Load base KnightLady textures (shared across variants)
knight_dir = SCRATCH / "ranger_export/MortalShell2/Content/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures"
tex_1001 = np.array(Image.open(knight_dir / "T_LadyKnight_1001_BC.png").convert("RGBA"), dtype=np.float32) / 255.0
tex_1002 = np.array(Image.open(knight_dir / "T_LadyKnight_1002_BC.png").convert("RGBA"), dtype=np.float32) / 255.0
tex_1003 = np.array(Image.open(knight_dir / "T_LadyKnight_1003_BC.png").convert("RGBA"), dtype=np.float32) / 255.0
tex_1004 = np.array(Image.open(knight_dir / "T_LadyKnight_1004_BC.png").convert("RGBA"), dtype=np.float32) / 255.0

# Pre-calculate masks
def armor_masks(tex):
    val = tex[..., :3].max(axis=2)
    metal_mask = smooth(val, 0.42, 0.65) * tex[..., 3]
    base_mask = (1.0 - smooth(val, 0.42, 0.65)) * tex[..., 3]
    return base_mask, metal_mask

mask_1001_armor, mask_1001_metal = armor_masks(tex_1001)
mask_1002_armor, mask_1002_metal = armor_masks(tex_1002)
mask_1003_cloth, mask_1003_metal = armor_masks(tex_1003)
mask_1004_armor, mask_1004_metal = armor_masks(tex_1004)

variants_info = [
    {
        "id": "dark-elf-ranger",
        "name": "Dark Elf Ranger",
        "input": "../original/extracted/dark-elf-ranger/Dark Elf Ranger",
        "smert_path": SCRATCH / "ranger_export/MortalShell2/Content/Sparta/Characters/Shells/Smert/Mesh/Textures/T_Smert_HoodRobe_01_BC.png",
        "cape_gain": 2.6,
        "cape_default": [0.1725, 0.1961, 0.2510, 1.0],
    },
    {
        "id": "dark-elf-ranger-light",
        "name": "Dark Elf Ranger Light",
        "input": "../original/extracted/dark-elf-ranger-light/Dark Elf Ranger Light",
        "smert_path": SCRATCH / "ranger_light_export/MortalShell2/Content/Sparta/Characters/Shells/Smert/Mesh/Textures/T_Smert_HoodRobe_01_BC.png",
        "cape_gain": 1.8,
        "cape_default": [0.6118, 0.5686, 0.5137, 1.0],
    },
    {
        "id": "rogue-knight-lady",
        "name": "Rogue Knight Lady",
        "input": "../original/extracted/rogue-knight-lady/RogueKnightLady",
        "smert_path": SCRATCH / "rogue_export/MortalShell2/Content/Sparta/Characters/Shells/Smert/Mesh/Textures/T_Smert_HoodRobe_01_BC.png",
        "cape_gain": 2.6,
        "cape_default": [0.1647, 0.1725, 0.1922, 1.0],
    },
]

variants_config = []

palettes = [
    {
        "id": "crimson-vanguard",
        "name": "Crimson Vanguard",
        "values": {
            "armor": [0.48, 0.12, 0.18, 1.0],
            "metal": [0.84, 0.70, 0.32, 1.0],
            "clothing": [0.22, 0.08, 0.12, 1.0],
            "cape": [0.55, 0.10, 0.16, 1.0],
        },
    },
    {
        "id": "midnight-phantom",
        "name": "Midnight Phantom",
        "values": {
            "armor": [0.18, 0.22, 0.32, 1.0],
            "metal": [0.78, 0.84, 0.92, 1.0],
            "clothing": [0.10, 0.12, 0.18, 1.0],
            "cape": [0.16, 0.20, 0.34, 1.0],
        },
    },
    {
        "id": "gilded-ashen",
        "name": "Gilded Ashen",
        "values": {
            "armor": [0.28, 0.27, 0.29, 1.0],
            "metal": [0.82, 0.65, 0.35, 1.0],
            "clothing": [0.16, 0.15, 0.17, 1.0],
            "cape": [0.78, 0.74, 0.68, 1.0],
        },
    },
    {
        "id": "verdigris-ranger",
        "name": "Verdigris Ranger",
        "values": {
            "armor": [0.14, 0.32, 0.26, 1.0],
            "metal": [0.76, 0.62, 0.40, 1.0],
            "clothing": [0.10, 0.18, 0.14, 1.0],
            "cape": [0.18, 0.36, 0.24, 1.0],
        },
    },
    {
        "id": "abyssal-eclipse",
        "name": "Abyssal Eclipse",
        "values": {
            "armor": [0.12, 0.11, 0.14, 1.0],
            "metal": [0.80, 0.58, 0.72, 1.0],
            "clothing": [0.18, 0.09, 0.22, 1.0],
            "cape": [0.28, 0.12, 0.32, 1.0],
        },
    },
]

for v in variants_info:
    v_dir = AUTHORING / f"colors-{v['id']}"
    v_dir.mkdir(parents=True, exist_ok=True)

    # Write KnightLady dye layers
    dye_1004_armor = write_dye_layer(v_dir, tex_1004[..., :3], mask_1004_armor, 2.4)
    dye_1004_metal = write_dye_layer(v_dir, tex_1004[..., :3], mask_1004_metal, 1.4)

    dye_1001_armor = write_dye_layer(v_dir, tex_1001[..., :3], mask_1001_armor, 2.4)
    dye_1001_metal = write_dye_layer(v_dir, tex_1001[..., :3], mask_1001_metal, 1.4)

    dye_1002_armor = write_dye_layer(v_dir, tex_1002[..., :3], mask_1002_armor, 2.4)
    dye_1002_metal = write_dye_layer(v_dir, tex_1002[..., :3], mask_1002_metal, 1.4)

    dye_1003_cloth = write_dye_layer(v_dir, tex_1003[..., :3], mask_1003_cloth, 2.4)
    dye_1003_metal = write_dye_layer(v_dir, tex_1003[..., :3], mask_1003_metal, 1.4)

    # Write Smert cloak dye layer
    tex_smert = np.array(Image.open(v["smert_path"]).convert("RGBA"), dtype=np.float32) / 255.0
    mask_smert = np.ones(tex_smert.shape[:2], dtype=np.float32) * tex_smert[..., 3]
    dye_cape = write_dye_layer(v_dir, tex_smert[..., :3], mask_smert, v["cape_gain"])

    controls = [
        {
            "id": "armor",
            "name": "Armor Plates",
            "group": "outfit",
            "role": "garment",
            "kind": "color",
            "hue_locked": False,
            "default": [0.45098, 0.46667, 0.49412, 1.0],
        },
        {
            "id": "metal",
            "name": "Ornaments & Filigree",
            "group": "outfit",
            "role": "metal",
            "kind": "color",
            "hue_locked": True,
            "default": [0.74510, 0.77255, 0.78039, 1.0],
        },
        {
            "id": "clothing",
            "name": "Waist Sash",
            "group": "outfit",
            "role": "accent",
            "kind": "color",
            "hue_locked": False,
            "default": [0.14510, 0.13333, 0.16863, 1.0],
        },
        {
            "id": "cape",
            "name": "Cloak & Hood",
            "group": "outfit",
            "role": "garment",
            "kind": "color",
            "hue_locked": False,
            "default": v["cape_default"],
        },
    ]

    surfaces = [
        {
            "id": "armor-1004",
            "slots": [0],
            "parameter": "BaseColorMap  non VT",
            "resolution": 2048,
            "layers": {
                "armor": dye_1004_armor,
                "metal": dye_1004_metal,
            },
        },
        {
            "id": "armor-1001",
            "slots": [1],
            "parameter": "BaseColorMap  non VT",
            "resolution": 2048,
            "layers": {
                "armor": dye_1001_armor,
                "metal": dye_1001_metal,
            },
        },
        {
            "id": "armor-1002",
            "slots": [2],
            "parameter": "BaseColorMap  non VT",
            "resolution": 2048,
            "layers": {
                "armor": dye_1002_armor,
                "metal": dye_1002_metal,
            },
        },
        {
            "id": "waist-1003",
            "slots": [3],
            "parameter": "BaseColorMap  non VT",
            "resolution": 2048,
            "layers": {
                "clothing": dye_1003_cloth,
                "metal": dye_1003_metal,
            },
        },
        {
            "id": "cloak",
            "slots": [6],
            "parameter": "BaseColorMap  non VT",
            "resolution": 2048,
            "layers": {
                "cape": dye_cape,
            },
        },
    ]

    colors_data = {
        "id": "dantemk2.beauteknightproxima",
        "colors": {
            "schema": 1,
            "controls": controls,
            "surfaces": surfaces,
            "palettes": palettes,
        },
    }

    (v_dir / "colors.json").write_text(json.dumps(colors_data, indent=2) + "\n")

    variants_config.append({
        "id": v["id"],
        "name": v["name"],
        "inputs": [v["input"]],
        "mesh": "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/SK_Shell_KnightLady_V04",
        "materials": "materials.json",
        "include": [
            "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_1001",
            "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_1002",
            "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_1003",
            "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_1004",
            "/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_Proxima_HandSpear_01",
            "/Game/Sparta/Characters/Shells/Smert/Mesh/Textures/MI_Smert_HoodRobe_01",
        ],
        "colors": f"colors-{v['id']}/colors.json",
    })

(AUTHORING / "variants.json").write_text(json.dumps(variants_config, indent=2) + "\n")
print("Authoring files created successfully.")
