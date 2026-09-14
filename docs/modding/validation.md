# Guide and utility validation

Local checks on 14 September 2026:

- Python 3.14.7: 38 packaging/project/color/source tests passed, including interrupted ZIP writes, duplicate output refusal, project path resolution and protection of per-variant colors.
- A project built from the original BeauteKnightLady source completed conversion, embedded its color recipe/resources, verified the trio and read back the release ZIP with matching bytes. Evidence is ignored under `work/modding-guide-check/`.
- The relocated CSSAuthoring editor module compiled successfully against the installed UE 5.6.1 Linux editor, all eight actions. This did not recook or install an outfit.
- Public guide links resolve inside the repository or to explicitly cited upstream documentation. Authoring source/configuration is tracked; engine assets, intermediate output and editor caches are ignored.

No game installation was changed by these packaging checks. The rebuilt KnightLady package has not been reinstalled or given a new gameplay acceptance claim. The earlier outfit gameplay checks remain separate evidence. Native Windows conversion, arbitrary new skeletons/shaders and a complete general Unreal starter-content SDK remain unverified.
