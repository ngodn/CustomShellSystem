"""Resolve UE 5.6.1 DX12 material texture registers from extracted shader code.

Reads FShaderResourceTable, checks its byte round trip and joins its layout
hash to the cooked uniform expression set. Supports standard 2D/cube textures.
It does not infer texture identity from DXIL sample order.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


ARRAYS = ("srv", "sampler", "uav", "layout_hashes", "texture", "collection")


def resource_table(data, container_offset):
    cursor = 0

    def read(fmt):
        nonlocal cursor
        size = struct.calcsize(fmt)
        if cursor + size > container_offset:
            raise ValueError("Resource table exceeds the declared container offset")
        values = struct.unpack_from(fmt, data, cursor)
        cursor += size
        return values

    bits, = read("<I")
    arrays = {}
    for name in ARRAYS:
        count, = read("<i")
        if not 0 <= count <= 65536:
            raise ValueError("Invalid resource array count")
        arrays[name] = list(read(f"<{count}I"))
    if cursor != container_offset or data[cursor:cursor + 4] != b"DXBC":
        raise ValueError("Resource table does not end at the verified DXBC container")
    encoded = struct.pack("<I", bits)
    for name in ARRAYS:
        values = arrays[name]
        encoded += struct.pack("<i", len(values)) + struct.pack(f"<{len(values)}I", *values)
    if encoded != data[:cursor]:
        raise ValueError("Resource table round trip failed")
    count = len(arrays["layout_hashes"])
    if count > 32 or bits >> count:
        raise ValueError("Resource bitmask exceeds the layout table")
    decoded = {}
    for name in ("srv", "sampler", "uav", "texture", "collection"):
        stream = arrays[name]
        entries = []
        if stream:
            if len(stream) <= count or stream[-1] != 0xffffffff:
                raise ValueError("Missing resource stream terminator")
            tokens = stream[count:-1]
            if tokens != sorted(tokens) or len(tokens) != len(set(tokens)):
                raise ValueError("Invalid resource token order")
            offsets = [0] * count
            for index, token in enumerate(tokens, count):
                buffer = token >> 24
                if buffer >= count or not bits & (1 << buffer):
                    raise ValueError("Token references an inactive buffer")
                if not offsets[buffer]:
                    offsets[buffer] = index
                entries.append({"buffer": buffer, "resource": (token >> 8) & 65535,
                                "bind": token & 255})
            if offsets != stream[:count]:
                raise ValueError("Resource stream offset table mismatch")
        decoded[name] = entries
    return arrays["layout_hashes"], decoded


def resolve(shader, readbacks, directory):
    path = (directory / shader["uecode"]).resolve()
    if path.parent != directory:
        raise ValueError("Shader path escapes its extraction directory")
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest().upper() != shader["uecode_sha256"]:
        raise ValueError("Shader extraction hash mismatch")
    offset, size = shader["container_offset"], shader["container_bytes"]
    if not 0 <= offset < len(data) or not 0 < size <= len(data) - offset:
        raise ValueError("Invalid DXBC extent")
    if hashlib.sha256(data[offset:offset + size]).hexdigest().upper() != shader["container_sha256"]:
        raise ValueError("DXBC hash mismatch")
    hashes, table = resource_table(data, offset)
    maps = [resource["LoadedShaderMap"] for obj in readbacks[shader["package"]]
            for resource in obj.get("LoadedMaterialResources", [])
            if resource["LoadedShaderMap"]["ResourceHash"] == shader["map_hash"]]
    if len(maps) != 1:
        raise ValueError("Expected one matching cooked shader map")
    material_map = maps[0]
    if material_map["ShaderPlatform"] != shader["platform"]:
        raise ValueError("Shader platform mismatch")
    expressions = material_map["Content"]["MaterialCompilationOutput"]["UniformExpressionSet"]
    layout = expressions["UniformBufferLayoutInitializer"]
    slots = [i for i, value in enumerate(hashes) if value == layout["Hash"]]
    if layout["Name"] != "Material" or len(slots) != 1:
        raise ValueError("Expected one verified Material uniform buffer")
    slot = slots[0]
    groups = expressions["UniformTextureParameters"]
    if len(groups) < 2 or any(groups[2:]):
        raise ValueError("Only standard 2D/cube texture groups are supported")
    if expressions["UniformExternalTextureParameters"] or expressions["UniformTextureCollectionParameters"]:
        raise ValueError("External textures and collections need explicit support")
    resources = layout["Resources"]
    parameters = {}
    for kind, group in zip(("2d", "cube"), groups[:2]):
        for texture in group:
            index = len(parameters) * 2
            if index + 1 >= len(resources):
                raise ValueError("Truncated material resource layout")
            expected = expressions["UniformPreshaderBufferSize"] * 16 + index * 8
            if resources[index]["MemberOffset"] != expected or resources[index + 1]["MemberOffset"] != expected + 8:
                raise ValueError("Unexpected texture/sampler resource offsets")
            if resources[index + 1]["MemberType"] != "UBMT_SAMPLER":
                raise ValueError("Texture lacks its adjacent sampler")
            parameters[index] = {"dimension": kind, **texture}
    bindings = []
    for entry in table["texture"]:
        if entry["buffer"] == slot:
            if entry["resource"] not in parameters:
                raise ValueError("Texture token has no matching material expression")
            bindings.append({"register": f"t{entry['bind']}", "resource_index": entry["resource"],
                             **parameters[entry["resource"]]})
    return {"package": shader["package"], "map_hash": shader["map_hash"],
            "resource_index": shader["resource_index"], "uecode_sha256": shader["uecode_sha256"],
            "material_buffer": slot, "material_layout_hash": layout["Hash"],
            "header_round_trip": True, "bindings": bindings}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("readback", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    manifest = json.loads(args.manifest.read_text())
    readbacks = json.loads(args.readback.read_text())
    results = [resolve(shader, readbacks, args.manifest.resolve().parent) for shader in manifest["shaders"]]
    with args.output.open("x") as stream:
        json.dump({"scope": "Static DX12 texture-register mapping. No graph reconstruction or runtime changes.",
                   "shaders": results}, stream, indent=2)
        stream.write("\n")
    print(json.dumps({"shaders": len(results), "texture_bindings": sum(len(r["bindings"]) for r in results)}))


if __name__ == "__main__":
    main()
