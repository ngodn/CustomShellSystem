//! Narrow material-header relocation using the pinned Retoc serializer.
//! Rust 2024, Retoc d7b6350 plus the local audited conversion patches.
use anyhow::{Context, Result, ensure};
use retoc::legacy_asset::{FLegacyPackageHeader, FPackageNameMap};
use retoc::logging::{Log, LogLevel, NoopLogBackend};
use retoc::version::EngineVersion;
use serde::Deserialize;
use serde_json::json;
use std::{collections::BTreeMap, fs, io::Cursor, path::PathBuf, sync::Arc};

#[derive(Deserialize)]
struct Job { input: PathBuf, output: PathBuf }
#[derive(Deserialize)]
struct Request { mapping: BTreeMap<String, String>, jobs: Vec<Job>, report: PathBuf }

fn read(bytes: &[u8]) -> Result<FLegacyPackageHeader> {
    let mut header = FLegacyPackageHeader::deserialize(
        &mut Cursor::new(bytes), Some(EngineVersion::UE5_6.package_file_version()))?;
    ensure!(header.summary.versioning_info.legacy_file_version == -9, "Expected UE5.6");
    ensure!(header.summary.is_filter_editor_only(), "Cooked headers only");
    ensure!(header.summary.versioning_info.total_header_size as usize == bytes.len(), "Header length mismatch");
    ensure!(header.cell_exports.is_empty() && header.cell_imports.is_empty() && header.data_resources.is_empty(), "Material-only layout required");
    ensure!(header.summary.soft_object_paths.count == 0, "Out-of-line soft paths unsupported");
    ensure!(header.exports.len() == 1, "Expected a single material export");
    let export = &header.exports[0];
    ensure!(export.class_index.is_import(), "Material must import its class");
    let class = &header.imports[export.class_index.to_import_index() as usize];
    ensure!(header.name_map.get(class.object_name)? == "MaterialInstanceConstant", "Expected MaterialInstanceConstant");
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

fn relocate(header: &mut FLegacyPackageHeader, mapping: &BTreeMap<String, String>) {
    header.summary.package_name = rename(&header.summary.package_name, mapping);
    header.name_map = FPackageNameMap::create_from_names(
        header.name_map.copy_raw_names().iter().map(|n| rename(n, mapping)).collect());
}

fn main() -> Result<()> {
    let args: Vec<_> = std::env::args_os().collect();
    ensure!(args.len() == 2, "Usage: rewrite_material request.json");
    let request: Request = serde_json::from_slice(&fs::read(&args[1])?)?;
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
        let mut header = read(&input)?;
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
        relocate(&mut header, &request.mapping);
        let output = write(&header, None)?;
        let mut back = read(&output)?;
        ensure!(back.summary.package_name == request.mapping[&old_package], "Package readback mismatch");
        let names = back.name_map.copy_raw_names();
        ensure!(!names.iter().any(|n| request.mapping.keys().any(|old| n == old || n.starts_with(&format!("{old}.")))), "Stale mapped reference");
        relocate(&mut back, &inverse);
        ensure!(write(&back, Some(input.len()))? == input, "Inverse relocation differs from original");
        reports.push(json!({"input":job.input,"output":job.output,"old":old_package,
            "package":header.summary.package_name,"old_header_bytes":input.len(),
            "new_header_bytes":output.len(),"payload_bytes":payload.len(),
            "unchanged_roundtrip":true,"inverse_roundtrip":true}));
        staged.push((job.output.clone(),output,payload));
    }
    for (path, bytes, payload) in staged {
        fs::create_dir_all(path.parent().context("Output needs a parent")?)?;
        // Exclusive files preserve an earlier candidate, even if created meanwhile.
        use std::io::Write;
        fs::OpenOptions::new().write(true).create_new(true).open(&path)?.write_all(&bytes)?;
        fs::OpenOptions::new().write(true).create_new(true).open(path.with_extension("uexp"))?.write_all(&payload)?;
    }
    fs::write(request.report, serde_json::to_vec_pretty(&json!({"passed":true,"materials":reports}))?)?;
    Ok(())
}
