# Modder kit and editor workflow

Queued 2026-09-21 at the user's request, after the first SeduXtress Black Pearl workflow stabilizes. This does not replace the active path migration or the full Next-Gen objective.

The user selected `/home/eins0fx/development/mods/msII/CSS-Modding` as the home for this work and requested a separate Git repository published publicly to their GitHub account. Set up the repository when this phase starts, confirm the account and existing remote from authenticated GitHub state, and use coherent commits. Publish reusable code, documentation and distributable starter content; keep game-derived assets and private reference data out. The repository and remote have not been created yet.

## Intended deliverables

1. A versioned authoring standard: skeleton and bone conventions, units and transforms, mesh binding and retargeting, material slots, morphs, modular outfit parts, physics and motion interfaces, package identity and short `/Game/CSS/` paths. Distinguish the visual mesh reference pose from the animation skeleton reference that repaired V44.
2. Reusable starter files for Blender and Unreal, with a simple distributable example, standard rig, export settings, recipe templates and package metadata. Separate redistributable starter content from assets that modders must obtain from their own game installation. Preserve a known-good minimal example and a full modular example.
3. A written guide that a new modder can follow from a clean workspace to a verified `.pak/.ucas/.utoc` trio. Pin supported tool versions, document installation and troubleshooting, and explain each validation failure in terms of a concrete repair.
4. A Blender add-on for the proven workflow: source validation, rig and weight checks, modular part/material/morph definitions, physics chain setup and validated export. Preserve the author's proportions and original source files.
5. An Unreal editor plugin for validated import, skeleton and post-process binding, physics/material setup, short-path checks, cooking, recipe validation and trio packaging. Keep editor tooling separate from game runtime components.
6. A recorded end-to-end tutorial using those same starter files and tools. Show real Blender edits, Unreal import/setup, successful validation, package generation, installation and in-game results. Include chapters and matching written steps; do not substitute generated video for actual tool operation.

## Order and acceptance

Stabilize and record the manual pipeline first. Use it to define the plugin interfaces and automate operations without silently changing their behavior. Then have a clean-workspace trial establish that the starter files, guide and plugins produce matching valid outputs without relying on private caches, absolute developer paths or undocumented assets.

The kit should cover the full CSS modular physics/motion architecture, not just a static character replacement. Include regression checks for hands and weapon postures, missing collisions, virtual bones, proportions, material references, outfit toggles, morph extremes, grounding, lifecycle and performance. Keep the distinction between verified behavior and remaining release coverage clear.

No guide, starter-kit release, add-on, editor plugin or tutorial is claimed complete by this scope note. The current V44 gameplay acceptance is valuable evidence for Black Pearl, not blanket certification of every future mod or weapon combination.
