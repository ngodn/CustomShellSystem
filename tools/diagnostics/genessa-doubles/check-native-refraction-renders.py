"""Check that zero-alpha eyes still refract, and zero fade removes distortion."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import OpenEXR


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory",type=Path)
    args=parser.parse_args()
    root=args.directory.resolve(strict=True)
    report=json.loads((root/"captures.json").read_text())
    if not report["source_files_unchanged"] or not report["compilation"]["passed"]:
        raise ValueError("Source or compilation failure")
    states={"removed","zero_fade","full","half","neutral_ior","reflection_zero","visible_surface"}
    groups={form:{} for form in ("faithful","stray")}
    for row in report["images"]:
        if row["form"] not in groups or row["state"] not in states or row["state"] in groups[row["form"]]:
            raise ValueError("Unexpected or duplicate capture")
        path=(root/row["image"]).resolve(strict=True)
        if path.parent!=root or path.suffix!=".exr" or hashlib.sha256(path.read_bytes()).hexdigest()!=row["sha256"]:
            raise ValueError("Invalid capture path or hash")
        with OpenEXR.File(str(path)) as image:
            rgb=image.channels()["RGBA"].pixels[:,:,:3].astype(np.float64)
        if rgb.shape!=(report["resolution"],report["resolution"],3) or not np.isfinite(rgb).all():
            raise ValueError("Invalid capture pixels")
        groups[row["form"]][row["state"]]=rgb
    results=[]
    for form,images in groups.items():
        if set(images)!=states:
            raise ValueError("Incomplete captures")
        background=images["removed"]
        differences={state:float(np.max(np.abs(value-background))) for state,value in images.items()}
        distorted=np.max(np.abs(images["full"]-background),axis=2)>1e-5
        added=(images["visible_surface"]-images["full"]).mean(axis=(0,1))
        checks={"patterned_background":bool(background.max()-background.min()>.1),
            "zero_fade_equals_removed":bool(np.array_equal(images["zero_fade"],background)),
            "neutral_ior_equals_removed":bool(np.array_equal(images["neutral_ior"],background)),
            "zero_opacity_still_refracts":bool(np.count_nonzero(distorted)>20),
            "half_fade_still_refracts":differences["half"]>1e-5,
            "fade_changes_distortion":bool(np.max(np.abs(images["half"]-images["full"]))>1e-5),
            "reflection_zero_preserves_distortion":bool(np.array_equal(images["reflection_zero"],images["full"])),
            "positive_opacity_renders_ghost":bool(np.max(np.abs(images["visible_surface"]-images["full"]))>1e-5),
            "added_surface_tint":bool(added[2]>added[1]>added[0] if form=="faithful" else added[0]>max(added[1:]))}
        results.append({"form":form,"passed":all(checks.values()),"checks":checks,
            "distorted_pixels":int(np.count_nonzero(distorted)),"background_difference":differences,
            "added_surface_mean_rgb":added.tolist()})
    result={"passed":all(r["passed"] for r in results),"forms":results,"scope":report["scope"]}
    with (root/"hdr-check.json").open("x") as stream:stream.write(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2))
    if not result["passed"]:raise SystemExit(1)


if __name__=="__main__":
    main()
