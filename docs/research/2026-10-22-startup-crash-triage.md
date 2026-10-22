# Reported startup allocator crash, October 22

Status: queued investigation, not reproduced or fixed. The user asks to handle
this later and fix or prevent it if practical. No game restart, settings change,
binary replacement or release change was made for this initial triage.

## Report

SamFisher91, CSS Nexus comments, October 22 2026 at 12:28 PM (timestamp supplied
by the user): startup crashes after the latest game update; removing UE4SS lets
the game start. Reporter says they use the UE4SS version from the mod page, but
does not identify the archive or DLL revision. Reported exception:

```text
EXCEPTION_ACCESS_VIOLATION reading address 0x00000000000d0003
MortalShell2_Win64_Shipping!FMallocBinned2::Free()
Core/Public/HAL/MallocBinned2.h:434
UE4SS
```

This is a partial stack. Removing UE4SS also removes execution of its loaded
mods, so that test alone does not distinguish loader failure from a mod failure.
The allocator frame identifies the reported failure site, not who produced the
bad memory state. No ranked cause or allocator workaround is justified yet.

## Local evidence

Read-only receipts: `work/startup-crash-26073152/inventory.json` and
`UE4SS-observed.log`. Prior gameplay coverage is recorded in the
[sidearm audit](2026-10-22-sidearm-audit.md).

- Installed UE4SS matches `native/ue4ss-runtime.json`:
  `3.0.1-1111-g97b7e501`, SHA-256
  `fb1839ee91f71f83d508d44a2763a15ac1bb0c5fb4e504ac0fcfca64376a054a`.
- Installed UE4SS.dll, dwmapi.dll and UE4SS-settings.ini match the supplied
  `UE4SS For MS2 - NO AOB 45 3 2026-09-17T13-49Z R0FFdhg1M.zip`.
- Installed `UE4SS_Signatures/StaticConstructObject.lua` matches the older
  `UE4SS For MS2 45 1.1 2026-08-22T00-28Z LV22MDK54.zip`. The newer archive does
  not contain that file. This is an observed mixed installation, not a proven
  fault. Preserve it when describing the working baseline.
- The observed October 22 07:09 UTC startup log reports engine 5.6, a successful
  pattern scan including GMalloc, and the Lua StaticConstructObject override.
  It also warns that EngineTick scan/vtable addresses differ. Installed settings
  select the Scan method. Neither successful scanning nor that warning alone
  establishes the reporter's cause. Prior local gameplay succeeded on build
  26073152 under Wine; it is not a Windows reproduction of this report.
- All 62 saved local CrashContext.runtime-xml files parsed: 47 GPUCrash and
  15 Crash. None contains `FMallocBinned2` or the reported read address. These
  historical records do not exhaust all possible crashes or unsaved dumps.
- CSS 1.0.2's ZIP contains no UE4SS.dll, dwmapi.dll or UE4SS-settings.ini.
  The loader package is a separate prerequisite. Reporter package identity must
  be obtained rather than inferred from their installed CSS version.

## Evidence needed and next experiment

Obtain the exact UE4SS archive/version, game build, OS, full crash stack, and
UE4SS.log from the failed launch before another launch overwrites it. Include
the associated crash dump if generated, settings, signature/layout overrides,
mods.txt, enabled mod list and any additional mod search paths. Dumps may contain
private process data; use a private transfer and redact personal paths from
public logs. A screenshot of the short allocator frame is not enough.

On an installation that reproduces the same exception, back up configuration
and saves, preserve each launch's evidence, and compare:

1. The original failing configuration, confirming the same signature.
2. The same loader/configuration with all UE4SS mods disabled, including
   enabled.txt discovery and additional search paths. Merely editing mods.txt
   is insufficient for our installed configuration, whose log shows both paths.
3. If loader-only succeeds, restore dependency-complete mod groups to isolate
   the smallest failing set. If loader-only fails, investigate loader/config
   compatibility separately, retaining the exact game executable and dump.

Do not load or save gameplay with temporarily missing content. Restore the
original configuration after each bounded test. Treat different crashes as
separate failures. Change one variable at a time; do not recommend arbitrary
GMalloc signatures, cache toggles or a runtime upgrade from this short stack.
Any eventual runtime change needs native ABI and gameplay verification for CSS
and CSSX, not just a successful title screen.

## Upstream context

UE4SS explicitly does not promise universal plug-and-play compatibility and
notes that game-specific signatures can need updates. It is not independent of
all game changes. [Official project README](https://github.com/UE4SS-RE/RE-UE4SS#targeting-ue-versions-from-47-to-58).
Its installation documentation distinguishes the loader, working directory,
configuration and mods. [Official installation guide](https://docs.ue4ss.com/installation-guide.html).
These explain the investigation boundaries, not a fix for this report.

## Suggested reporter reply, not posted

Thanks for reporting this. Could you send the exact UE4SS download/version,
your game build, UE4SS.log from the crashed launch, and the full crash report
or dump? Please also include your installed mod list and any UE4SS configuration
changes. Removing UE4SS disables all mods that use it, so we still need to
separate a loader issue from a mod issue. I haven't reproduced this crash on
the updated game yet.
