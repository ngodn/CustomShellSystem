"""Exercise the packaging boundary with real manifest and menu validation."""
import importlib.util
import json
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("cssx_package", repo / "tools/cssx_package.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
validator = Path(sys.argv[1])
with tempfile.TemporaryDirectory(prefix="cssx-打包-") as temporary:
    root = Path(temporary)
    source = root / "源文件"
    shutil.copytree(repo / "examples/extensions/lua-counter", source)
    for name in ("state/private.json", "logs/current.jsonl", "cache/shadow.dll"):
        file = source / name
        file.parent.mkdir(exist_ok=True)
        file.write_text("must not ship")
    output = module.build(source, root / "发布", validator, "CSSX_{name}_{version}")
    first = output.read_bytes()
    with zipfile.ZipFile(output) as archive:
        assert set(archive.namelist()) == {
            "examples.counter/extension.json", "examples.counter/main.lua", "examples.counter/menu.json"
        }
    assert module.build(source, root / "发布", validator, "CSSX_{name}_{version}").read_bytes() == first
    for name in ("../outside.dll", "C:/file", "CON.txt", "state/private.json", "a./b", "a\\b", "a:b", "./main.lua"):
        try:
            module.package_path(name)
        except ValueError:
            pass
        else:
            raise AssertionError(name)
    manifest = json.loads((source / "extension.json").read_text())
    manifest["files"] = ["state/private.json"]
    (source / "extension.json").write_text(json.dumps(manifest))
    try:
        module.build(source, root / "发布", validator, "CSSX_{name}_{version}")
    except ValueError:
        pass
    else:
        raise AssertionError("Generated state was allowed into the ZIP")
    assert output.read_bytes() == first, "A failed build replaced the verified ZIP"
print("Minimal reproducible ZIP, Unicode paths, rejected paths and existing artifact retention passed")
