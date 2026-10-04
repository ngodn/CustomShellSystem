"""Compare the reconstructed response with the shipped DXIL arithmetic.

Requires private shader readback files. Does not run or modify the game.
The HLSL scalar response is compiled as C++23 for numerical checks; DXC also
compiles the complete shader, including noise projection, as a pixel shader.
Texture sampling, uniform binding and final rendered equivalence remain separate.
"""
import argparse
import functools
import hashlib
import json
import math
from pathlib import Path
import random
import re
import struct
import subprocess


def f32(value):
    return struct.unpack("f", struct.pack("f", value))[0]


def saturate(value):
    return 0.0 if math.isnan(value) else min(max(value, 0.0), 1.0)


def divide(a, b):
    if b == 0:
        return math.nan if a == 0 else math.copysign(math.inf, a)
    return a / b


class Shader:
    def __init__(self, path):
        self.definitions = dict(re.findall(r"^  (%\d+) = (.+)$", path.read_text(), re.M))

    def response(self, inputs, corrupted):
        noise, facing, local_y, radius, opacity, depth, depth_fade, eye = inputs
        root = f32(math.sqrt(saturate(opacity)))
        seeds = {294: noise, 302: facing, 225: local_y}
        keys = (404, 406, 428, 450, 343) if corrupted else (409, 411, 433, 455, 347)
        seeds.update(zip(keys, (radius, f32(root * root), depth, depth_fade, eye)))
        seeds = {f"%{key}": value for key, value in seeds.items()}

        @functools.cache
        def value(token):
            if token in seeds:
                return seeds[token]
            if not token.startswith("%"):
                return f32(struct.unpack(">d", bytes.fromhex(token[2:]))[0] if token.startswith("0x") else float(token))
            line = self.definitions[token].split(" ;", 1)[0].strip()
            if m := re.fullmatch(r"f(add|sub|mul|div) fast float (\S+), (\S+)", line):
                a, b = value(m[2]), value(m[3])
                return f32({"add": lambda: a + b, "sub": lambda: a - b,
                            "mul": lambda: a * b, "div": lambda: divide(a, b)}[m[1]]())
            if m := re.fullmatch(r"fcmp fast (ole|oge) float (\S+), (\S+)", line):
                a, b = value(m[2]), value(m[3])
                return a <= b if m[1] == "ole" else a >= b
            if m := re.fullmatch(r"select i1 (\S+), float (\S+), float (\S+)", line):
                return value(m[2] if value(m[1]) else m[3])
            if m := re.fullmatch(r"call float @dx.op.(unary|binary|dot4).f32\(i32 (\d+), (.+)\)", line):
                operands = [value(x) for x in re.findall(r"float ([^, )]+)", m[3])]
                op = int(m[2])
                if m[1] == "dot4":
                    products = [f32(a * b) for a, b in zip(operands[:4], operands[4:])]
                    return functools.reduce(lambda a, b: f32(a + b), products)
                if m[1] == "binary":
                    return {35: max, 36: min}[op](*operands)
                unary = {6: abs, 7: saturate, 21: lambda x: 2 ** x,
                         23: lambda x: math.log2(x) if x > 0 else -math.inf,
                         24: math.sqrt}
                return f32(unary[op](operands[0]))
            raise ValueError(f"Unsupported instruction required by response: {token} {line}")

        outputs = (344, 345, 345, 455) if corrupted else (348, 349, 350, 460)
        return [value(f"%{key}") for key in outputs]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shaders", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--dxc", type=Path, required=True)
    parser.add_argument("--cxx", default="c++")
    args = parser.parse_args()
    args.output.mkdir(exist_ok=False, parents=True)
    kernel = Path(__file__).with_name("native-ghost-response.hlsl").resolve()
    paths = [args.shaders / (name + ".ll") for name in (
        "5ABE009F6CA07E8779EF333B384E0BF4D5944A91-4",
        "FBF9FDC100C1D350095BDA08A4F43A40EF6790A0-9")]
    # SSA identifiers are build-specific. Refuse another game's shader by accident.
    expected_hashes = ("1267d8d0470c084767f31c25d5c80260", "64e6ad0fa5bb429754b43e24daeaf4e8")
    for path, expected in zip(paths, expected_hashes):
        if expected and f"; shader hash: {expected}" not in path.read_text():
            raise ValueError(f"Unexpected shader: {path}")
    host = args.output / "response.cpp"
    host.write_text('''#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
using std::abs; using std::sqrt; using std::exp2; using std::log2;
using std::min; using std::max;
float saturate(float x) { return std::isnan(x) ? 0.0f : std::clamp(x, 0.0f, 1.0f); }
struct float4 { float x,y,z,w; float4(float a,float b,float c,float d):x(a),y(b),z(c),w(d) {} };
#define CSS_ASTRAL_RESPONSE_ONLY
''' + '#include ' + json.dumps(str(kernel)) + '''
int main() {
    float n, f, y, r, o, d, z, e; int c;
    std::cout << std::setprecision(9);
    while (std::cin >> n >> f >> y >> r >> o >> d >> z >> e >> c) {
        auto v = CSSAstralResponse(n,f,y,r,o,d,z,e,c != 0);
        std::cout << v.x << ' ' << v.y << ' ' << v.z << ' ' << v.w << '\\n';
    }
}
''')
    binary = args.output / "response"
    subprocess.run([args.cxx, "-std=c++23", "-O2", "-ffp-contract=off", "-Wall", "-Wextra", "-Werror",
                    str(host), "-o", str(binary)], check=True, timeout=60)
    harness = args.output / "response.hlsl"
    harness.write_text('#include ' + json.dumps(str(kernel)) + '''
Texture2D Noise : register(t0); SamplerState NoiseSampler : register(s0);
float4 MainPS(float4 a : TEXCOORD0, float4 b : TEXCOORD1, float4 c : TEXCOORD2) : SV_Target {
    float n = CSSAstralNoise(Noise, NoiseSampler, a.xyz, b.xyz, c.x);
    return CSSAstralResponse(n, b.w, a.y, c.y, c.z, a.w, c.w, 1.0f, c.x < 0);
}
''')
    subprocess.run([str(args.dxc.resolve()), "-T", "ps_6_6", "-E", "MainPS", "-WX",
                    str(harness), "-Fo", str(args.output / "response.dxil")], check=True, timeout=60)
    rng = random.Random(20261004)
    cases = [[rng.random(), rng.uniform(-1, 1), rng.uniform(-0.2, 1.4), rng.random(),
              rng.uniform(-0.1, 1.1), rng.uniform(0, 2048), rng.random(), rng.uniform(0.01, 16)]
             for _ in range(4096)]
    for opacity in (0, 0.0001, 0.5, 0.9999, 1):
        for radius in (0, 0.5, 1):
            for facing in (-1, 0, 1):
                for noise in (0, 0.5, 1):
                    cases.append([noise, facing, 0.75, radius, opacity, 300, 1, 1])
    cases = [[f32(x) for x in row] for row in cases]
    input_text = "".join(" ".join(map(str, row + [corrupted])) + "\n"
                         for corrupted in (0, 1) for row in cases)
    actual = subprocess.run([str(binary.resolve())], input=input_text, text=True,
                            capture_output=True, check=True, timeout=60).stdout.splitlines()
    if len(actual) != len(cases) * 2:
        raise RuntimeError("Incomplete host output")
    maximum_error = 0.0
    for corrupted, path in enumerate(paths):
        shader = Shader(path)
        for index, row in enumerate(cases):
            expected = shader.response(row, corrupted)
            got = [float(x) for x in actual[corrupted * len(cases) + index].split()]
            if len(got) != 4:
                raise RuntimeError("Expected RGBA output")
            for channel, (a, b) in enumerate(zip(expected, got)):
                error = abs(a - b)
                if not math.isfinite(b) or error > 1e-5 * max(1, abs(a)):
                    raise AssertionError(f"form={corrupted} case={index} channel={channel} expected={a} got={b} input={row}")
                maximum_error = max(maximum_error, error)
    report = {"cases_per_form": len(cases), "forms": 2, "max_absolute_error": maximum_error,
              "shader_model": "ps_6_6", "kernel_sha256": hashlib.sha256(kernel.read_bytes()).hexdigest(),
              "oracle_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
              "limits": "Scalar arithmetic comparison and HLSL compilation only. Noise sampling, UE binding and image equivalence are not verified."}
    (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
