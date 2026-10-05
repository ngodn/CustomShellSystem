# EveTest asset-load report

Didodiodi reported `Asset could not load: /Game/CSS/EveTest/` on October 5,
2026. The complete asset name, installed versions and logs are unavailable.
The cause remains unconfirmed. Do not describe this as a fixed runtime bug or
as proven user error.

The actual Eve v1.3.0 and v1.4.0 ZIPs were checked. Each catalog has 17 asset
references, all present with the requested export names. The cooked containers
contain 330 and 331 packages respectively, including all five EveTest outfit
meshes and their physics/animation packages. Public import hashes resolve for
1,152 and 1,147 package imports respectively. The input providers contain only
that Eve release and base-game containers, with no other outfit or CSS shared
packages. Archive UCAS bytes match the independently hashed local payloads.

This rules out absent catalog assets and unresolved checked package imports in
these two release ZIPs. It does not reproduce the reporter's installation or
prove that Unreal can execute every asset successfully. A mismatched, corrupt,
unmounted or conflicting installation remains possible; logs are needed to
choose among those explanations. The EveTest namespace itself is valid.

## Reproducing the offline check

`tools/AssetDependencyAudit` targets .NET 10 and the repo's CUE4Parse dependency.
Build it with `dotnet build tools/AssetDependencyAudit -c Release`, then run:

```text
AssetDependencyAudit CONTAINERS MAPPINGS PACKAGES REFERENCES OUTPUT
```

CONTAINERS must contain the release trio plus the base game's global and
pakchunk containers. Do not include other mods. PACKAGES lists the package
names independently recovered from the release container; REFERENCES lists
catalog object paths. Both lists contain one entry per line. OUTPUT must be a
new JSON file. The tool rejects missing exports and unresolved public imports;
it does not deserialize all export payloads or run engine code.

Final tool validation passes both releases. A deliberately nonexistent export
in the real bikini package returns failure, confirming that the export check
can detect a broken catalog reference. A separate earlier missing-base test
using only the bikini mesh passed because its direct imports are included in
Eve; that experiment is not evidence of missing-base detection.

Local evidence: `work/evetest-load-audit1/`, including archive readback hashes,
package inventories, final-dependencies.json for both versions and
negative-export.json. These diagnostics are not shipped in the runtime ZIP.

The author requested a reply to the reporter instead of adding troubleshooting
to the BBCode changelog. No BBCode change was made for this report. The reply
asks for CSS/Eve versions and CSS.log/UE4SS.log if reinstalling all three Eve
files together with the game closed does not resolve it.
