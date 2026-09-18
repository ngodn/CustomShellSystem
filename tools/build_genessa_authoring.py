#!/usr/bin/env python3
"""Author dye recipes and authoring configuration for MoreBeauteGenessa.

Creates:
- authoring/materials.json
- authoring/variants.json
- authoring/colors-original/ (colors.json + hashed dye PNGs)
- authoring/colors-lite/ (colors.json + hashed dye PNGs)
"""
from pathlib import Path
import json
import hashlib
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
import sys

PORT = Path('/home/eins0fx/development/mods/msII/CSS-eins0fx-collections/ports/CSS_MoreBeauteGenessa_dantemk2')
SCRATCH = Path('/home/eins0fx/.gemini/antigravity-cli/brain/5d48e097-d29c-4116-a195-17848c60af29/scratch')
sys.path.insert(0, '/home/eins0fx/development/mods/msII/CustomShellSystem/tools')
from build_color_recipes import layer, smooth, rgba
from css_colors import validate

SIZE = 2048
OUTFIT_ID = 'dantemk2.morebeautegenessa'

def main():
    auth_dir = PORT / 'authoring'
    auth_dir.mkdir(parents=True, exist_ok=True)
    
    # 1. materials.json
    materials = {
        "0": "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_top.MI_Genessa_top",
        "1": "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_top.MI_Genessa_top",
        "2": "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_top.MI_Genessa_top",
        "3": "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_body.MI_Genessa_body",
        "7": "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_body.MI_Genessa_body",
        "8": "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_skirt_NEW.MI_Genessa_skirt_NEW",
        "9": "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_skirt_NEW.MI_Genessa_skirt_NEW"
    }
    (auth_dir / 'materials.json').write_text(json.dumps(materials, indent=2) + '\n')
    print('Wrote materials.json')

    # Load textures
    top_bc = np.array(Image.open(SCRATCH / 'export_pngs/MortalShell2/Content/MS1/Characters/NPC/Genessa/Textures/Genessa_top_BC.png').convert('RGBA').resize((SIZE, SIZE), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0
    top_brm = np.array(Image.open(SCRATCH / 'export_pngs/MortalShell2/Content/MS1/Characters/NPC/Genessa/Textures/Genessa_top_BRM.png').convert('RGB').resize((SIZE, SIZE), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0

    skirt_bc = np.array(Image.open(SCRATCH / 'export_pngs/MortalShell2/Content/MS1/Characters/NPC/Genessa/Textures/skirt_BC.png').convert('RGBA').resize((SIZE, SIZE), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0
    skirt_brm = np.array(Image.open(SCRATCH / 'export_pngs/MortalShell2/Content/MS1/Characters/NPC/Genessa/Textures/skirt_BRM.png').convert('RGB').resize((SIZE, SIZE), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0

    orig_bc = np.array(Image.open(SCRATCH / 'export_pngs/MortalShell2/Content/MS1/Characters/NPC/Genessa/Textures/Genessa_body_BC.png').convert('RGBA').resize((SIZE, SIZE), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0
    orig_brm = np.array(Image.open(SCRATCH / 'export_pngs/MortalShell2/Content/MS1/Characters/NPC/Genessa/Textures/Genessa_body_BRM.png').convert('RGB').resize((SIZE, SIZE), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0

    lite_bc = np.array(Image.open(SCRATCH / 'export_pngs_lite/MortalShell2/Content/MS1/Characters/NPC/Genessa/Textures/Genessa_body_BC.png').convert('RGBA').resize((SIZE, SIZE), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0
    lite_brm = np.array(Image.open(SCRATCH / 'export_pngs_lite/MortalShell2/Content/MS1/Characters/NPC/Genessa/Textures/Genessa_body_BRM.png').convert('RGB').resize((SIZE, SIZE), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0

    # Controls definition
    controls = [
        {
            "id": "clothing",
            "name": "Garment",
            "role": "garment",
            "group": "outfit",
            "default": rgba("242B32")
        },
        {
            "id": "metal",
            "name": "Ornaments",
            "role": "metal",
            "group": "outfit",
            "default": rgba("C5B98C")
        },
        {
            "id": "face",
            "name": "Porcelain Mask",
            "role": "face",
            "group": "body",
            "default": rgba("D4CDC0")
        },
        {
            "id": "skin",
            "name": "Complexion",
            "role": "skin",
            "group": "body",
            "default": rgba("A9BACB")
        },
        {
            "id": "eye-glow",
            "name": "Eyes",
            "role": "eye-glow",
            "group": "body",
            "default": rgba("CCE5FF"),
            "bindings": [
                {"slot": 5, "parameter": "Color"},
                {"slot": 10, "parameter": "Color"}
            ]
        },
        {
            "id": "eye-intensity",
            "name": "Eye Glow",
            "type": "scalar",
            "kind": "intensity",
            "group": "body",
            "default": [1.5, 0, 0, 1],
            "max": 5,
            "step": 0.05,
            "bindings": [
                {"slot": 5, "parameter": "Intensity"},
                {"slot": 10, "parameter": "Intensity"}
            ]
        }
    ]

    # 5 Palettes
    palettes = [
        {
            "id": "crimson",
            "name": "Crimson Regalia",
            "values": {
                "clothing": rgba("6B1828"),
                "metal": rgba("D4AF37"),
                "face": rgba("ECE3D2"),
                "skin": rgba("B5AAB3"),
                "eye-glow": rgba("FF3344"),
                "eye-intensity": [2.2, 0, 0, 1]
            }
        },
        {
            "id": "midnight",
            "name": "Midnight Silver",
            "values": {
                "clothing": rgba("182236"),
                "metal": rgba("C5D3E8"),
                "face": rgba("DFE3EB"),
                "skin": rgba("B2BFD0"),
                "eye-glow": rgba("33D6FF"),
                "eye-intensity": [2.0, 0, 0, 1]
            }
        },
        {
            "id": "obsidian",
            "name": "Gilded Obsidian",
            "values": {
                "clothing": rgba("1C1A1F"),
                "metal": rgba("D2A33A"),
                "face": rgba("DDD7CC"),
                "skin": rgba("9F9EA3"),
                "eye-glow": rgba("FFAA11"),
                "eye-intensity": [2.5, 0, 0, 1]
            }
        },
        {
            "id": "verdigris",
            "name": "Verdigris Sanctuary",
            "values": {
                "clothing": rgba("1B4840"),
                "metal": rgba("BA9656"),
                "face": rgba("DBE3DD"),
                "skin": rgba("BAC4BF"),
                "eye-glow": rgba("2EE89A"),
                "eye-intensity": [1.8, 0, 0, 1]
            }
        },
        {
            "id": "twilight",
            "name": "Ashen Twilight",
            "values": {
                "clothing": rgba("381E40"),
                "metal": rgba("D59B8B"),
                "face": rgba("E5DFE8"),
                "skin": rgba("B5A5BC"),
                "eye-glow": rgba("C942FF"),
                "eye-intensity": [2.2, 0, 0, 1]
            }
        }
    ]

    # Shared Top layers
    face_img = Image.new('L', (SIZE, SIZE))
    ImageDraw.Draw(face_img).ellipse((SIZE * 0.49, SIZE * 0.155, SIZE * 0.653, SIZE * 0.3), fill=255)
    face_mask = np.array(face_img.filter(ImageFilter.GaussianBlur(SIZE / 2048)), dtype=np.float32) / 255.0
    top_metal_mask = smooth(top_brm[..., 2], 0.2, 0.65) * (1.0 - face_mask)
    top_cloth_mask = (1.0 - top_metal_mask) * (1.0 - face_mask)

    # Shared Skirt layers
    skirt_metal_mask = smooth(skirt_brm[..., 2], 0.2, 0.65)
    skirt_cloth_mask = 1.0 - skirt_metal_mask

    # Original Body layers
    orig_metal_mask = smooth(orig_brm[..., 2], 0.2, 0.65)
    orig_lmax = orig_bc[..., :3].max(-1)
    orig_skin_mask = (1.0 - orig_metal_mask) * smooth(orig_bc[..., 2] - orig_bc[..., 0], 0.015, 0.065) * smooth(orig_lmax, 0.22, 0.45)
    orig_cloth_mask = (1.0 - orig_metal_mask) * (1.0 - orig_skin_mask)

    # Lite Body layers
    lite_metal_mask = smooth(lite_brm[..., 2], 0.2, 0.65)
    diff = np.max(np.abs(orig_bc[..., :3] - lite_bc[..., :3]), axis=-1)
    torso_skin = (1.0 - lite_metal_mask) * smooth(diff, 0.05, 0.15) * smooth(lite_bc[..., :3].min(-1), 0.25, 0.45)
    lite_skin_base = (1.0 - lite_metal_mask) * smooth(lite_bc[..., 2] - lite_bc[..., 0], 0.015, 0.065) * smooth(lite_bc[..., :3].max(-1), 0.22, 0.45)
    lite_skin_mask = np.clip(lite_skin_base + torso_skin, 0.0, 1.0)
    lite_cloth_mask = (1.0 - lite_metal_mask) * (1.0 - lite_skin_mask)

    # Function to save hashed dye layer
    def save_dye(out_dir, rgb, mask, gain):
        temp_file = out_dir / 'temp_dye.png'
        layer(temp_file, rgb, mask, gain, optimize=False)
        content = temp_file.read_bytes()
        digest = hashlib.sha256(content).hexdigest()[:32]
        target_name = f'dye-{digest}.png'
        target_path = out_dir / target_name
        temp_file.replace(target_path)
        return target_name

    for variant_id, body_bc, body_cloth_m, body_metal_m, body_skin_m in [
        ('original', orig_bc, orig_cloth_mask, orig_metal_mask, orig_skin_mask),
        ('lite', lite_bc, lite_cloth_mask, lite_metal_mask, lite_skin_mask)
    ]:
        v_dir = auth_dir / f'colors-{variant_id}'
        v_dir.mkdir(parents=True, exist_ok=True)

        top_cloth_file = save_dye(v_dir, top_bc[..., :3], top_cloth_mask * top_bc[..., 3], 2.4)
        top_metal_file = save_dye(v_dir, top_bc[..., :3], top_metal_mask * top_bc[..., 3], 1.4)
        top_face_file = save_dye(v_dir, top_bc[..., :3], face_mask * top_bc[..., 3], 1.2)

        body_cloth_file = save_dye(v_dir, body_bc[..., :3], body_cloth_m * body_bc[..., 3], 2.4)
        body_metal_file = save_dye(v_dir, body_bc[..., :3], body_metal_m * body_bc[..., 3], 1.4)
        body_skin_file = save_dye(v_dir, body_bc[..., :3], body_skin_m * body_bc[..., 3], 1.2)

        skirt_cloth_file = save_dye(v_dir, skirt_bc[..., :3], skirt_cloth_mask * skirt_bc[..., 3], 2.4)
        skirt_metal_file = save_dye(v_dir, skirt_bc[..., :3], skirt_metal_mask * skirt_bc[..., 3], 1.4)

        surfaces = [
            {
                "id": "top",
                "slots": [0, 1, 2],
                "parameter": "BaseColorMap  non VT",
                "resolution": SIZE,
                "layers": {
                    "clothing": top_cloth_file,
                    "metal": top_metal_file,
                    "face": top_face_file
                }
            },
            {
                "id": "body",
                "slots": [3, 7],
                "parameter": "BaseColorMap  non VT",
                "resolution": SIZE,
                "layers": {
                    "clothing": body_cloth_file,
                    "metal": body_metal_file,
                    "skin": body_skin_file
                }
            },
            {
                "id": "skirt",
                "slots": [8, 9],
                "parameter": "BaseColorMap  non VT",
                "resolution": SIZE,
                "layers": {
                    "clothing": skirt_cloth_file,
                    "metal": skirt_metal_file
                }
            }
        ]

        colors_data = {
            "schema": 1,
            "controls": controls,
            "surfaces": surfaces,
            "palettes": palettes
        }
        
        # Validate schema and layers
        validate(colors_data)
        recipe = {"id": OUTFIT_ID, "colors": colors_data}
        (v_dir / 'colors.json').write_text(json.dumps(recipe, indent=2) + '\n')
        print(f'Validated and wrote colors-{variant_id}/colors.json')

    # Write variants.json
    variants_spec = [
        {
            "id": "original",
            "name": "Original",
            "inputs": [
                "../original/extracted/original/Beaute Genessa"
            ],
            "mesh": "/Game/Sparta/Characters/NPCs/SesterGenessa/Art/Mesh/SK_Sester_Genessa_V6",
            "materials": "materials.json",
            "include": [
                "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_top",
                "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_body",
                "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_skirt_NEW"
            ],
            "colors": "colors-original/colors.json"
        },
        {
            "id": "lite",
            "name": "Lite",
            "inputs": [
                "../original/extracted/lite/Beaute Genessa Lite"
            ],
            "mesh": "/Game/Sparta/Characters/NPCs/SesterGenessa/Art/Mesh/SK_Sester_Genessa_V6",
            "materials": "materials.json",
            "include": [
                "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_top",
                "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_body",
                "/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_skirt_NEW"
            ],
            "colors": "colors-lite/colors.json"
        }
    ]
    (auth_dir / 'variants.json').write_text(json.dumps(variants_spec, indent=2) + '\n')
    print('Wrote variants.json')

if __name__ == '__main__':
    main()
