"""Read source indices from shared facial candidates; never save assets."""
import hashlib
import json
import os
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[3]
WORK = Path(os.environ["CSS_ANIM_WORK"]).resolve(strict=True)
CONTENT = ROOT.parent / "CSS-eins0fx-collections/tools/CSSAuthoring/Content"
if not WORK.is_relative_to(ROOT / "work/cine-animation-port"):
    raise ValueError("Use the private animation workspace")
evidence_path = WORK.parent / "shared-faces-v1/facial-catalog-evidence.json"
evidence = json.loads(evidence_path.read_text())
staging = json.loads((WORK.parent / "shared-faces-v1/facial-stage-verify.json").read_text())
maps = []
protected = {}
for row in staging["results"]:
    package = row["target"]
    source = CONTENT / (package.removeprefix("/Game/") + ".uasset")
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    if evidence["protected"].get(str(source)) != digest:
        raise ValueError("Facial candidate changed since verification: " + package)
    protected[str(source)] = digest
    mesh = unreal.load_asset(package)
    if not mesh:
        raise ValueError("Facial candidate is missing")
    result = json.loads(unreal.CSSFacialLibrary.expression_vertex_map(mesh))
    if result["mesh"] != package + "." + package.rsplit("/", 1)[-1]:
        raise ValueError("Vertex map came from a different mesh")
    maps.append(result)
    print("FACIAL_VERTEX_MAP", package, len(result["source_vertices"]), flush=True)
if len(maps) != 6 or any(hashlib.sha256(Path(p).read_bytes()).hexdigest() != h for p, h in protected.items()):
    raise ValueError("Incomplete mapping or modified source asset")
with (WORK / "vertex-maps.json").open("x") as output:
    json.dump({"complete": True, "maps": maps, "protected": protected}, output, separators=(",", ":"))
    output.write("\n")
