"""Compare eye reflection coordinates and IOR inputs with the shipped DXIL."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import subprocess

spec = importlib.util.spec_from_file_location("eyes", Path(__file__).with_name("check-native-eye-response.py"))
eyes = importlib.util.module_from_spec(spec)
spec.loader.exec_module(eyes)
f32 = eyes.f32


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("basepass", type=Path)
    parser.add_argument("distortion", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--dxc", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(exist_ok=False)
    kernel = Path(__file__).with_name("native-refraction-response.hlsl").resolve()
    # Match the extracted container hash, not a filename alone.
    expected = {args.basepass: "90facf98a2b6d1af6f2e0bc4638e235dda601c487516f6e91ad5b48b4a053a45"}
    manifest = json.loads((args.distortion.parent / "manifest.json").read_text())
    rows = [r for r in manifest["shaders"] if r["resource_index"] == 22 and
            r["map_hash"] == "2CF1A2C5FF20EC86B9FBCE98AEA8D1AC7F06C2A3"]
    if len(rows) != 1:
        raise ValueError("Expected one native refraction distortion shader")
    expected[args.distortion] = rows[0]["container_sha256"].lower()
    for path, digest in expected.items():
        if hashlib.sha256(path.with_suffix("").read_bytes()).hexdigest() != digest:
            raise ValueError(f"Unexpected native shader: {path}")
    cpp = args.output / "response.cpp"
    cpp.write_text('''#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
using std::max; using std::abs; using std::cos; using std::sin; using std::exp2; using std::log2;
struct float4 { float x,y,z,w; float4(float a,float b,float c,float d):x(a),y(b),z(c),w(d){} };
''' + '#include ' + json.dumps(str(kernel)) + '''
int main() {
 float v[9]; std::cout << std::setprecision(9);
 while (std::cin >> v[0]) {
  for (int i=1;i<9;++i) if (!(std::cin >> v[i])) return 2;
  auto r=CSSRefractionDirection(v[0],v[1],v[2],v[3],v[4],v[5],v[6]);
  std::cout << r.x << ' ' << r.y << ' ' << r.z << ' ' << CSSRefractionDelta(v[7],v[8]) << '\\n';
 }
}
''')
    binary = args.output / "response"
    subprocess.run(["c++", "-std=c++23", "-O2", "-ffp-contract=off", "-Wall", "-Wextra", "-Werror",
                    str(cpp), "-o", str(binary)], check=True, timeout=60)
    hlsl = args.output / "response.hlsl"
    hlsl.write_text('#include ' + json.dumps(str(kernel)) + '''
float4 MainPS(float4 a:TEXCOORD0,float4 b:TEXCOORD1,float4 c:TEXCOORD2):SV_Target {
 return CSSRefractionDirection(a.x,a.y,a.z,a.w,b.x,b.y,b.z) + CSSRefractionDelta(b.w,c.x);
}
''')
    subprocess.run([str(args.dxc.resolve()), "-T", "ps_6_6", "-E", "MainPS", "-WX", str(hlsl),
                    "-Fo", str(args.output / "response.dxil")], check=True, timeout=60)
    rng = random.Random(20261005)
    cases = [[rng.uniform(-1,1) for _ in range(8)] + [rng.uniform(.8,2)] for _ in range(512)]
    cases += [[.2,.3,.4,0,0,0,0,facing,ior] for facing in (-1,0,.5,1)
              for ior in (1,1.33,1.3683993)]
    cases = [[f32(x) for x in row] for row in cases]
    actual = subprocess.run([str(binary.resolve())], input="".join(" ".join(map(str,row))+"\n" for row in cases),
                            text=True, capture_output=True, check=True, timeout=60).stdout.splitlines()
    if len(actual) != len(cases):
        raise ValueError("Incomplete arithmetic output")
    maximum = 0.
    for v, line in zip(cases, actual, strict=True):
        seeds = {303:v[0],304:v[1],305:v[2],322:f32(v[3]*f32(6.2831854820251465)),
                 308:f32(v[4]*f32(6.2831854820251465)),333:f32(v[5]*f32(6.2831854820251465)),307:v[6]}
        want = eyes.evaluate(args.basepass, {f"%{k}":x for k,x in seeds.items()}, (354,363,373))
        want += eyes.evaluate(args.distortion, {"%175":v[7],"%188":v[8]}, (190,))
        got = [float(x) for x in line.split()]
        if len(got) != 4:
            raise ValueError("Invalid response")
        for a,b in zip(got,want,strict=True):
            error=abs(a-b)
            maximum=max(maximum,error)
            if not eyes.math.isfinite(a) or not eyes.math.isfinite(b) or error > 2e-6 + abs(b)*3e-5:
                raise AssertionError((v,got,want))
    result = {"passed": True, "cases": len(cases), "maximum_absolute_error": maximum,
              "dxc_sm6_compiled": True,
              "sources": {str(p):hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in (kernel,args.basepass,args.distortion)},
              "scope": "Native reflection direction and refraction-delta arithmetic. Not texture filtering, scene rendering, ghost composition or runtime proof."}
    (args.output / "result.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2))


if __name__ == "__main__":
    main()
