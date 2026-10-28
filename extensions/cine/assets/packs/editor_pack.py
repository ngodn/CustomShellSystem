"""Author or independently reload one pack in Unreal Engine 5.6.1 Python."""
import json
import os
from pathlib import Path
import sys

import unreal

STAGE = Path(unreal.Paths.project_dir()).resolve()
SPEC = json.loads((STAGE / "input.json").read_text())
ROWS = json.loads((STAGE / "rows.json").read_text())
ROOT = "/Game/CSSX/CINE/" + SPEC["pack"]
MODE = os.environ["CINE_PACK_MODE"]
LIB = unreal.EditorAssetLibrary
TABLES = unreal.DataTableFunctionLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
STRUCT = unreal.load_object(None, "/Script/GameplayTags.GameplayTagTableRow")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def inspect_audio(sound, expected):
    actual = {
        "asset": sound.get_path_name(),
        "seconds": sound.get_editor_property("duration"),
        "rate": sound.get_editor_property("imported_sample_rate"),
        "sample_rate": sound.get_editor_property("sample_rate"),
        "channels": sound.get_editor_property("num_channels"),
        "compression": str(sound.get_editor_property("sound_asset_compression_type")),
        "quality": sound.get_editor_property("compression_quality"),
        "loading": str(sound.get_editor_property("loading_behavior")),
        "looping": sound.get_editor_property("looping"),
        "sound_class": str(sound.get_editor_property("sound_class_object")),
    }
    require(actual["asset"] == expected["asset"], str(actual))
    require(abs(actual["seconds"] - expected["seconds"]) <= max(1 / expected["rate"], 0.00002), str(actual))
    require(actual["rate"] == expected["rate"] and actual["channels"] == expected["channels"], str(actual))
    require(sound.get_editor_property("sound_asset_compression_type") == unreal.SoundAssetCompressionType.BINK_AUDIO, str(actual))
    require(sound.get_editor_property("loading_behavior") == unreal.SoundWaveLoadingBehavior.INHERITED, str(actual))
    require(actual["quality"] == 80 and not actual["looping"], str(actual))
    require(sound.get_editor_property("sound_class_object") is None, str(actual))
    return actual


require(unreal.SystemLibrary.get_engine_version().startswith("5.6.1-"), "Expected UE 5.6.1")
require(MODE in ("author", "reload"), "Unknown mode")
if SPEC["audio"]:
    # -NoSound skips the audio device's normal decoder registration.
    unreal.load_module("BinkAudioDecoder")
if MODE == "author":
    require(not LIB.does_directory_exist(ROOT), "Authoring output already exists")
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", STRUCT)
    table = TOOLS.create_asset("DT_CINE_Pack", ROOT, unreal.DataTable, factory)
    require(table is not None, "DataTable creation failed")
    require(TABLES.fill_data_table_from_json_string(table, json.dumps(ROWS, ensure_ascii=False), STRUCT), "DataTable fill failed")
    require(LIB.save_loaded_asset(table, only_if_is_dirty=False), "DataTable save failed")
    for audio_id, expected in SPEC["audio"].items():
        factory = unreal.SoundFactory()
        factory.set_editor_property("auto_create_cue", False)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", expected["source"])
        task.set_editor_property("destination_path", ROOT + "/Audio")
        task.set_editor_property("destination_name", expected["asset"].rsplit(".", 1)[1])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("factory", factory)
        TOOLS.import_asset_tasks([task])
        sound = LIB.load_asset(expected["asset"])
        require(isinstance(sound, unreal.SoundWave), "SoundWave import failed: " + audio_id)
        sound.set_editor_property("sound_asset_compression_type", unreal.SoundAssetCompressionType.BINK_AUDIO)
        sound.set_editor_property("compression_quality", 80)
        # Streamed like the game's own waves. A ForceInline wave is looked up at runtime by a format key
        # built from the game's audio cook settings, which this project does not share, so it never played.
        sound.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.INHERITED)
        sound.set_editor_property("looping", False)
        sound.set_editor_property("sound_class_object", None)
        inspect_audio(sound, expected)
        require(LIB.save_loaded_asset(sound, only_if_is_dirty=False), "SoundWave save failed")

table = LIB.load_asset(ROOT + "/DT_CINE_Pack")
require(isinstance(table, unreal.DataTable), "DataTable reload failed")
require(table.get_editor_property("row_struct") == STRUCT, "Wrong row struct")
actual_rows = json.loads(TABLES.export_data_table_to_json_string(table))
expected_rows = {row["Name"]: row for row in ROWS}
require({row["Name"] for row in actual_rows} == set(expected_rows), "Row set differs")
for row in actual_rows:
    require(row["Tag"] in ("", "None"), "Nonempty gameplay tag")
    require(row["DevComment"].encode("utf-8") == expected_rows[row["Name"]]["DevComment"].encode("utf-8"), "Row bytes differ: " + row["Name"])
audio = {key: inspect_audio(LIB.load_asset(value["asset"]), value) for key, value in SPEC["audio"].items()}
report = {"mode": MODE, "engine": unreal.SystemLibrary.get_engine_version(), "python": sys.version,
          "row_struct": STRUCT.get_path_name(), "rows": actual_rows, "audio": audio}
(STAGE / (MODE + "-verified.json")).write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
