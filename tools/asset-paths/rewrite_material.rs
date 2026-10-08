//! Narrow cooked-header relocation using the pinned Retoc serializer.
//! Rust 2024, Retoc d7b6350 plus the local audited conversion patches.
use anyhow::{Context, Result, ensure};
use retoc::legacy_asset::{EObjectDataResourceVersion, FLegacyPackageHeader, FPackageNameMap};
use retoc::logging::{Log, LogLevel, NoopLogBackend};
use retoc::version::EngineVersion;
use serde::Deserialize;
use serde_json::json;
use std::{collections::BTreeMap, fs, io::Cursor, path::PathBuf, sync::Arc};

#[derive(Deserialize)]
struct Job { input: PathBuf, output: PathBuf }
#[derive(Deserialize)]
struct Request {
    mapping: BTreeMap<String, String>, jobs: Vec<Job>, report: PathBuf,
    #[serde(default)] textures: bool,
    #[serde(default)] animation_blueprints: bool,
    #[serde(default)] material_parents: bool,
}

fn read(bytes: &[u8], expected_class: &str) -> Result<FLegacyPackageHeader> {
    let mut header = FLegacyPackageHeader::deserialize(
        &mut Cursor::new(bytes), Some(EngineVersion::UE5_6.package_file_version()))?;
    ensure!(header.summary.versioning_info.legacy_file_version == -9, "Expected UE5.6");
    ensure!(header.summary.is_filter_editor_only(), "Cooked headers only");
    ensure!(header.summary.versioning_info.total_header_size as usize == bytes.len(), "Header length mismatch");
    ensure!(header.cell_exports.is_empty() && header.cell_imports.is_empty(), "Unsupported cell table layout");
    if expected_class == "Texture2D" {
        ensure!(header.data_resource_version == Some(EObjectDataResourceVersion::Initial), "Unsupported texture resource version");
        ensure!(!header.data_resources.is_empty(), "Texture resource table required");
        for resource in &header.data_resources {
            ensure!(resource.cooked_index.is_none() && resource.flags == 0 &&
                    resource.duplicate_serial_offset == -1 && resource.outer_index.index == 0,
                    "Unsupported texture resource metadata");
            // UE5.6.1 BulkData.h: inline + single-use, or separate end-of-file
            // payload + force-not-inline + no-offset-fixup. Both retain offsets
            // within their original .uexp/.ubulk streams when names grow.
            ensure!(matches!(resource.legacy_bulk_data_flags, 0x48 | 0x10501), "Unsupported texture bulk flags");
            ensure!(resource.serial_offset >= 0 && resource.serial_size > 0 &&
                    resource.raw_size == resource.serial_size, "Invalid texture resource extent");
        }
    } else {
        ensure!(header.data_resources.is_empty(), "Non-texture resource tables unsupported");
    }
    ensure!(header.summary.soft_object_paths.count == 0, "Out-of-line soft paths unsupported");
    if expected_class == "AnimBlueprintGeneratedClass" {
        let mut generated_classes = Vec::new();
        for (index, export) in header.exports.iter().enumerate() {
            if export.class_index.is_import() {
                let class = header.imports.get(export.class_index.to_import_index() as usize)
                    .context("Invalid imported export class")?;
                let name = header.name_map.get(class.object_name)?;
                ensure!(matches!(&*name, "AnimBlueprintGeneratedClass" | "Function" | "ScriptStruct"),
                        "Unsupported blueprint export class: {name}");
                if name == "AnimBlueprintGeneratedClass" { generated_classes.push(index); }
            }
        }
        ensure!(generated_classes.len() == 1, "Expected one animation generated class");
        let generated = generated_classes[0];
        let class_name = header.name_map.get(header.exports[generated].object_name)?;
        let expected_default = format!("Default__{class_name}");
        let mut defaults = 0;
        for export in &header.exports {
            if !export.class_index.is_import() {
                ensure!(export.class_index.is_export() &&
                        export.class_index.to_export_index() as usize == generated &&
                        header.name_map.get(export.object_name)? == expected_default,
                        "Unsupported blueprint local-class export");
                defaults += 1;
            }
        }
        ensure!(defaults == 1, "Expected one animation class default object");
    } else {
        ensure!(header.exports.len() == 1, "Expected a single export");
        let export = &header.exports[0];
        ensure!(export.class_index.is_import(), "Asset must import its class");
        let class = header.imports.get(export.class_index.to_import_index() as usize)
            .context("Invalid imported asset class")?;
        ensure!(header.name_map.get(class.object_name)? == expected_class, "Unexpected asset class");
    }
    // Retoc's writer accepts export offsets relative to the .uexp stream.
    let size = header.summary.versioning_info.total_header_size as i64;
    for export in &mut header.exports {
        ensure!(export.serial_offset >= size && export.serial_size >= 0, "Invalid export extent");
        export.serial_offset -= size;
    }
    Ok(header)
}

fn write(header: &FLegacyPackageHeader, desired: Option<usize>) -> Result<Vec<u8>> {
    let mut result = Cursor::new(Vec::new());
    header.serialize(&mut result, desired, &Log::new(LogLevel::Error, Arc::new(NoopLogBackend {})))?;
    Ok(result.into_inner())
}

fn rename(value: &str, mapping: &BTreeMap<String, String>) -> String {
    for (old, new) in mapping {
        if value == old { return new.clone(); }
        if value.strip_prefix(old).is_some_and(|tail| tail.starts_with('.') || tail.starts_with(':')) {
            return format!("{}{}", new, &value[old.len()..]);
        }
    }
    value.to_owned()
}

fn name_table_mapping(mapping: &BTreeMap<String, String>) -> Result<BTreeMap<String, String>> {
    let mut names = mapping.clone();
    for (old, new) in mapping {
        // A package FName can store MI_Layer_29 as MI_Layer plus number 30.
        // Keep every index and number intact, including references in .uexp.
        let mut old_names = FPackageNameMap::create();
        let mut new_names = FPackageNameMap::create();
        let old_name = old_names.store(old);
        let new_name = new_names.store(new);
        ensure!(old_name.number == new_name.number, "Relocation changes a name number");
        let old_base = old_names.copy_raw_names().remove(0);
        let new_base = new_names.copy_raw_names().remove(0);
        if let Some(previous) = names.insert(old_base.clone(), new_base.clone()) {
            ensure!(previous == new_base, "Conflicting numbered-name relocation: {old_base}");
        }
    }
    Ok(names)
}

fn relocate(header: &mut FLegacyPackageHeader, mapping: &BTreeMap<String, String>) -> Result<()> {
    let names = name_table_mapping(mapping)?;
    let original = header.name_map.clone();
    header.summary.package_name = rename(&header.summary.package_name, mapping);
    header.name_map = FPackageNameMap::create_from_names(
        original.copy_raw_names().iter().map(|n| rename(n, &names)).collect());
    for import in &header.imports {
        for name in [import.class_package, import.class_name, import.object_name] {
            let old = original.get(name)?;
            ensure!(header.name_map.get(name)? == rename(&old, mapping),
                    "Relocation changed an unmapped import name: {old}");
        }
    }
    for export in &header.exports {
        let old = original.get(export.object_name)?;
        ensure!(header.name_map.get(export.object_name)? == rename(&old, mapping),
                "Relocation changed an unmapped export name: {old}");
    }
    Ok(())
}

fn main() -> Result<()> {
    let args: Vec<_> = std::env::args_os().collect();
    ensure!(args.len() == 2, "Usage: rewrite_material request.json");
    let request: Request = serde_json::from_slice(&fs::read(&args[1])?)?;
    ensure!(u8::from(request.textures) + u8::from(request.animation_blueprints) +
            u8::from(request.material_parents) <= 1, "Select only one asset class");
    let expected_class = if request.textures { "Texture2D" }
        else if request.animation_blueprints { "AnimBlueprintGeneratedClass" }
        else if request.material_parents { "Material" }
        else { "MaterialInstanceConstant" };
    ensure!(!request.report.exists(), "Report already exists");
    let inverse: BTreeMap<_, _> = request.mapping.iter().map(|(a,b)| (b.clone(),a.clone())).collect();
    ensure!(inverse.len() == request.mapping.len(), "Relocation collision");
    for (old, new) in &request.mapping {
        ensure!(new.starts_with("/Game/CSS/") && new.len() <= 96, "Invalid destination");
        ensure!(old.rsplit('/').next() == new.rsplit('/').next(), "This tool preserves object names");
    }
    // Validate all inputs and round trips before writing any material.
    let mut staged = Vec::new();
    let mut reports = Vec::new();
    for job in &request.jobs {
        let input = fs::read(&job.input).with_context(|| job.input.display().to_string())?;
        let payload = fs::read(job.input.with_extension("uexp"))?;
        ensure!(!job.output.exists() && !job.output.with_extension("uexp").exists(), "Output already exists");
        let bulk = job.input.with_extension("ubulk");
        ensure!(!job.output.with_extension("ubulk").exists(), "Bulk output already exists");
        ensure!(!job.input.with_extension("uptnl").exists(), "Optional bulk chunks unsupported");
        ensure!(!job.input.with_extension("m.ubulk").exists(), "Memory-mapped bulk chunks unsupported");
        ensure!(request.textures || !bulk.exists(), "Material bulk payload unsupported");
        let mut header = read(&input, expected_class)?;
        for resource in &header.data_resources {
            let length = if resource.legacy_bulk_data_flags == 0x48 { payload.len() as u64 }
                         else { fs::metadata(&bulk)?.len() };
            let end = resource.serial_offset.checked_add(resource.serial_size)
                .context("Texture resource extent overflow")?;
            ensure!(end as u64 <= length, "Texture resource exceeds its payload stream");
        }
        ensure!(write(&header, Some(input.len()))? == input, "Unchanged round trip differs: {}", job.input.display());
        let old_package = header.summary.package_name.clone();
        ensure!(request.mapping.contains_key(&old_package), "Material is missing from relocation map");
        for old in request.mapping.keys() {
            ensure!(!payload.windows(old.len()).any(|w| w == old.as_bytes()), "Inline payload path requires another serializer");
            let wide: Vec<u8> = old.encode_utf16().flat_map(u16::to_le_bytes).collect();
            ensure!(!payload.windows(wide.len()).any(|w| w == wide), "Inline UTF16 path requires another serializer");
        }
        for export in &header.exports {
            ensure!((export.serial_offset + export.serial_size) as usize <= payload.len(), "Export exceeds payload");
        }
        relocate(&mut header, &request.mapping)?;
        let output = write(&header, None)?;
        let mut back = read(&output, expected_class)?;
        ensure!(back.summary.package_name == request.mapping[&old_package], "Package readback mismatch");
        let names = back.name_map.copy_raw_names();
        ensure!(!names.iter().any(|n| request.mapping.keys().any(|old| n == old || n.starts_with(&format!("{old}.")))), "Stale mapped reference");
        relocate(&mut back, &inverse)?;
        ensure!(write(&back, Some(input.len()))? == input, "Inverse relocation differs from original");
        reports.push(json!({"input":job.input,"output":job.output,"old":old_package,
            "package":header.summary.package_name,"old_header_bytes":input.len(),
            "new_header_bytes":output.len(),"payload_bytes":payload.len(),"class":expected_class,
            "bulk_bytes":if bulk.exists() { fs::metadata(&bulk)?.len() } else { 0 },
            "unchanged_roundtrip":true,"inverse_roundtrip":true}));
        staged.push((job.output.clone(),output,payload,bulk));
    }
    for (path, bytes, payload, bulk) in staged {
        fs::create_dir_all(path.parent().context("Output needs a parent")?)?;
        // Exclusive files preserve an earlier candidate, even if created meanwhile.
        use std::io::Write;
        fs::OpenOptions::new().write(true).create_new(true).open(&path)?.write_all(&bytes)?;
        fs::OpenOptions::new().write(true).create_new(true).open(path.with_extension("uexp"))?.write_all(&payload)?;
        if bulk.exists() {
            let mut source = fs::File::open(bulk)?;
            let mut target = fs::OpenOptions::new().write(true).create_new(true).open(path.with_extension("ubulk"))?;
            std::io::copy(&mut source, &mut target)?;
        }
    }
    let report = if request.textures { json!({"passed":true,"textures":reports}) }
                 else if request.animation_blueprints { json!({"passed":true,"animation_blueprints":reports}) }
                 else if request.material_parents { json!({"passed":true,"material_parents":reports}) }
                 else { json!({"passed":true,"materials":reports}) };
    fs::write(request.report, serde_json::to_vec_pretty(&report)?)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use retoc::legacy_asset::FObjectImport;

    #[test]
    fn numbered_package_import_keeps_index_number_and_roundtrips() -> Result<()> {
        let old = "/Game/CSS/CommanderWhite/Materials/MI_Layer_29";
        let new = "/Game/CSS/eins0fx/CommanderWhite/Materials/MI_Layer_29";
        let mut header = FLegacyPackageHeader::default();
        let class = header.name_map.store("Package");
        let object = header.name_map.store(old);
        assert_eq!(object.number, 30);
        header.imports.push(FObjectImport { class_package: class, class_name: class,
            object_name: object, ..Default::default() });
        let original = header.name_map.copy_raw_names();
        relocate(&mut header, &BTreeMap::from([(old.into(), new.into())]))?;
        assert_eq!(header.name_map.get(object)?, new);
        assert_eq!(header.imports[0].object_name.index, object.index);
        assert_eq!(header.imports[0].object_name.number, object.number);
        relocate(&mut header, &BTreeMap::from([(new.into(), old.into())]))?;
        assert_eq!(header.name_map.copy_raw_names(), original);
        Ok(())
    }

    #[test]
    fn conflicting_or_unmapped_numbered_siblings_are_rejected() {
        let old = "/Game/CSS/Old/MI_Layer_29";
        let new = "/Game/CSS/New/MI_Layer_29";
        let mapping = BTreeMap::from([(old.into(), new.into()),
            ("/Game/CSS/Old/MI_Layer_30".into(), "/Game/CSS/Other/MI_Layer_30".into())]);
        assert!(name_table_mapping(&mapping).is_err());
        let mut header = FLegacyPackageHeader::default();
        let class = header.name_map.store("Package");
        let object = header.name_map.store("/Game/CSS/Old/MI_Layer_30");
        header.imports.push(FObjectImport { class_package: class, class_name: class,
            object_name: object, ..Default::default() });
        assert!(relocate(&mut header, &BTreeMap::from([(old.into(), new.into())])).is_err());
    }

    #[test]
    fn zero_padded_names_and_object_paths_keep_their_spelling() -> Result<()> {
        let mapping = BTreeMap::from([
            ("/Game/CSS/Old/MI_01".into(), "/Game/CSS/New/MI_01".into()),
            ("/Game/CSS/Old/MI_1".into(), "/Game/CSS/New/MI_1".into())]);
        let names = name_table_mapping(&mapping)?;
        assert_eq!(rename("/Game/CSS/Old/MI_01.MI_01", &names), "/Game/CSS/New/MI_01.MI_01");
        assert_eq!(rename("/Game/CSS/Old/MI_10", &names), "/Game/CSS/Old/MI_10");
        Ok(())
    }
}
