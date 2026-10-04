"""Compare native eye response reconstruction against extracted DXIL arithmetic.

Checks response arithmetic and scrolling UVs with sampled noise as inputs.
Texture filtering and graph integration require separate renders.
"""
import argparse
import functools
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import random
import re
import struct
import subprocess


SPEC = importlib.util.spec_from_file_location("native_response", Path(__file__).with_name("check-native-ghost-response.py"))
response = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(response)
f32, saturate, divide = response.f32, response.saturate, response.divide


def evaluate(path, seeds, outputs):
    definitions = dict(re.findall(r"^  (%\d+) = (.+)$", path.read_text(), re.M))

    @functools.cache
    def value(token):
        if token in seeds:
            return seeds[token]
        if not token.startswith("%"):
            return f32(struct.unpack(">d", bytes.fromhex(token[2:]))[0] if token.startswith("0x") else float(token))
        line = definitions[token].split(" ;", 1)[0].strip()
        if m := re.fullmatch(r"f(add|sub|mul|div) fast float (\S+), (\S+)", line):
            a, b = value(m[2]), value(m[3])
            return f32({"add": lambda: a + b, "sub": lambda: a - b,
                        "mul": lambda: a * b, "div": lambda: divide(a, b)}[m[1]]())
        if m := re.fullmatch(r"fcmp fast (ole|oge) float (\S+), (\S+)", line):
            return value(m[2]) <= value(m[3]) if m[1] == "ole" else value(m[2]) >= value(m[3])
        if m := re.fullmatch(r"select i1 (\S+), float (\S+), float (\S+)", line):
            return value(m[2] if value(m[1]) else m[3])
        if m := re.fullmatch(r"call float @dx.op.(unary|binary|dot3).f32\(i32 (\d+), (.+)\)", line):
            operands = [value(x) for x in re.findall(r"float ([^, )]+)", m[3])]
            op = int(m[2])
            if m[1] == "dot3":
                return functools.reduce(lambda a, b: f32(a + b),
                    [f32(a * b) for a, b in zip(operands[:3], operands[3:])])
            if m[1] == "binary":
                return {35: max, 36: min}[op](*operands)
            return f32({6: abs, 7: saturate, 12: math.cos, 13: math.sin,
                        21: lambda x: 2 ** x, 22: lambda x: x - math.floor(x),
                        23: lambda x: math.log2(x) if x > 0 else -math.inf,
                        24: math.sqrt}[op](operands[0]))
        raise ValueError(f"Unsupported required instruction: {token} {line}")

    return [value(f"%{key}") for key in outputs]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shaders", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--dxc", required=True, type=Path)
    parser.add_argument("--cxx", default="c++")
    args = parser.parse_args()
    args.output.mkdir(exist_ok=False, parents=True)
    kernel = Path(__file__).with_name("native-eye-response.hlsl").resolve()
    eye = args.shaders / "376BF936FA4BD614F125AF91C1D196D4DA98589D-6.dxbc.ll"
    smoke = args.shaders / "4AE4A640F38D9B7CDAC4A36B69E91638D9AB59FC-10.dxbc.ll"
    for path, digest in ((eye, "7318e1e8da3dc7bc5b489a7ec304e5ea"), (smoke, "9f2fbcea9ceb66e8b7682e7b4c3a0a0a")):
        if f"; shader hash: {digest}" not in path.read_text():
            raise ValueError("Unexpected native shader version")
    cpp = args.output / "response.cpp"
    cpp.write_text('''#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
using std::max; using std::exp2; using std::log2; using std::sqrt; using std::fmod;
float saturate(float x) { return std::isnan(x) ? 0.f : std::clamp(x, 0.f, 1.f); }
struct float4 { float x,y,z,w; float4(float a,float b,float c,float d):x(a),y(b),z(c),w(d){} };
struct float2 { float x,y; float2(float a,float b):x(a),y(b){} };
''' + '#include ' + json.dumps(str(kernel)) + '''
int main() {
    float v[13]; int kind;
    std::cout << std::setprecision(9);
    while (std::cin >> kind) {
        for (float &x : v) if (!(std::cin >> x)) return 2;
        float4 r(0,0,0,1);
        if (kind == 0) r = CSSGenessaEyeResponse(v[0],v[1],v[2],v[3],v[4],v[5],v[6]);
        else if (kind == 1) r = CSSGenessaSmokeResponse(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12]);
        else { auto uv = CSSGenessaSmokeUV(float2(v[0],v[1]),v[2],v[3],v[4]); r = float4(uv.x,uv.y,0,1); }
        std::cout << r.x << ' ' << r.y << ' ' << r.z << ' ' << r.w << '\\n';
    }
}
''')
    binary = args.output / "response"
    subprocess.run([args.cxx, "-std=c++23", "-O2", "-ffp-contract=off", "-Wall", "-Wextra", "-Werror",
                    str(cpp), "-o", str(binary)], check=True, timeout=60)
    harness = args.output / "response.hlsl"
    harness.write_text('#include ' + json.dumps(str(kernel)) + '''
float4 MainPS(float4 a:TEXCOORD0, float4 b:TEXCOORD1, float4 c:TEXCOORD2, float4 d:TEXCOORD3):SV_Target {
    float2 uv = CSSGenessaSmokeUV(a.xy,b.x,b.y,b.z);
    return CSSGenessaEyeResponse(a.z,a.w,b.x,b.y,b.z,b.w,c.x)
        + CSSGenessaSmokeResponse(a.x,a.y,a.z,a.w,b.x,b.y,b.z,b.w,c.x,c.y,c.z,c.w,d.x)
        + float4(uv,0,0);
}
''')
    subprocess.run([str(args.dxc.resolve()), "-T", "ps_6_6", "-E", "MainPS", "-WX",
                    str(harness), "-Fo", str(args.output / "response.dxil")], check=True, timeout=60)
    rng = random.Random(20261005)
    eyes = [[rng.uniform(-1, 1), rng.uniform(.1, 10), rng.random(), rng.random(), rng.random(),
             rng.uniform(0, 20), rng.uniform(.01, 16)] + [0.] * 6 for _ in range(1024)]
    smokes = [[rng.random(), rng.random(), rng.random(), rng.random(), rng.random(), rng.uniform(-1, 1),
               rng.uniform(.1, 5), rng.uniform(0, 10), rng.random(), rng.random(), rng.random(),
               rng.uniform(0, 20), rng.uniform(.01, 16)] for _ in range(1024)]
    for facing in (-1., 0., 1.):
        for amount in (0., .5, 1.):
            eyes.append([facing, 6., amount, amount, amount, 1.5, 1.] + [0.] * 6)
            smokes.append([amount] * 5 + [facing, 2., 2.5, .598958, .7813, 1., 2., 1.])
    uvs = [[rng.random(),rng.random(),rng.random(),rng.uniform(.1,10),rng.uniform(-2000,2000)] + [0.] * 8
           for _ in range(1024)]
    for time in (-600., 0., 599.99, 600., 600.01, 1200.):
        uvs.append([.5,.5,.5,1.,time] + [0.] * 8)
    cases = [(kind, [f32(x) for x in row]) for kind, rows in enumerate((eyes, smokes, uvs)) for row in rows]
    inputs = "".join(str(kind) + " " + " ".join(map(str, row)) + "\n" for kind, row in cases)
    actual = subprocess.run([str(binary.resolve())], input=inputs, text=True, capture_output=True,
                            check=True, timeout=60).stdout.splitlines()
    if len(actual) != len(cases):
        raise RuntimeError("Incomplete response output")
    maximum_error = 0.
    for (kind, row), line in zip(cases, actual):
        if kind == 0:
            seeds = dict(zip((99,101,107,108,109,125,131), row[:7]))
            seeds.update({136: 0., 138: 0., 139: 0., 140: 0.})
            expected = evaluate(eye, {f"%{k}": v for k, v in seeds.items()}, (150,151,152)) + [1.]
        elif kind == 1:
            seeds = dict(zip((150,158,166,80,25,249,242,251,199,200,201,217,223), row))
            seeds.update({228: 0., 230: 0., 231: 0., 232: 0.})
            expected = evaluate(smoke, {f"%{k}": v for k, v in seeds.items()}, (239,240,241,256))
        else:
            seeds = dict(zip((79,80,26,139,128), row[:5]))
            expected = evaluate(smoke, {f"%{k}": v for k, v in seeds.items()}, (143,144)) + [0.,1.]
        result = list(map(float, line.split()))
        if len(result) != 4 or not all(math.isfinite(x) for x in result + expected):
            raise ValueError("Nonfinite or incomplete response")
        for a, b in zip(result, expected):
            maximum_error = max(maximum_error, abs(a-b))
            if abs(a-b) > 2e-6 + 3e-5 * abs(b):
                raise AssertionError((kind, row, result, expected))
    report = {"passed": True, "cases": len(cases), "maximum_absolute_error": maximum_error,
              "dxc_sm6_compiled": True, "source_sha256": {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in (eye, smoke, kernel)},
              "scope": "Eye emission, sampled-noise smoke response and scrolling UV arithmetic. Texture filtering, engine fog, ghost composition and full rendering remain pending."}
    (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k:v for k,v in report.items() if k not in ('source_sha256','scope')}))


if __name__ == "__main__":
    main()
