# Vanilla runtime boundary

Status: implements [#19](https://github.com/juv4uk/my-lisp-cyberpunk/issues/19)
(`CP-VANILLA-BOUNDARY`), a Phase A prerequisite of
[#27](https://github.com/juv4uk/my-lisp-cyberpunk/issues/27)
(`CP-VANILLA-EPIC`).

## The rule, precisely

**Runtime dependency** = anything that must exist inside the Cyberpunk
2077 install directory, or be loaded into `Cyberpunk2077.exe`'s address
space, for our integration to work at all.

**Build-time reference** = anything we read, copy data from, or consult
while building our own artifact, that the finished artifact does not
require the game to have installed.

The boundary test is mechanical, not a judgment call: after a clean
vanilla Cyberpunk 2077 install, run `git status`-style diff of the game
directory before and after our artifact runs. Every new file that diff
shows is ours, and only ours, or the boundary is violated.

## Dependency table

| Dependency | Build-time reference? | Runtime dependency? | Allowed? | Reason |
| --- | --- | --- | --- | --- |
| RED4ext SDK headers/docs (`adapter/deps/red4ext.sdk`) | Yes | No, once we stop shipping a RED4ext plugin | Yes, as reference only | Struct layouts and RTTI type names are useful to *read* while designing a probe; a finished vanilla artifact must not require `RED4ext.dll`/`winmm.dll` installed |
| `cyberpunk2077_addresses.json` (RED4ext-maintained public address/hash database) | Yes | No | Yes, as reference only | Same as above — informs where to look during RE, never installed |
| CET (Cyber Engine Tweaks) | No | — | **No** | Explicitly named as forbidden runtime dependency by the owner and by #18/#19's own scope |
| RED4ext loader (`RED4ext.dll`, `bin\x64\winmm.dll` proxy) | No | — | **No** | Same — this is exactly the "third-party mod framework" boundary exists to exclude |
| Codeware | No | — | **No** | Same |
| ArchiveXL / other REDmod-ecosystem frameworks | No | — | **No** | Same |
| `my-lisp.exe` (canonical `runtime/my-lisp` submodule build) | — | Yes | Yes | Ours; the entire point of the integration is to reach this binary |
| Our own sidecar/probe executable (#29/#30/#31) | — | Yes | Yes | Ours; must not itself require any of the forbidden frameworks to run |
| Our own minimal in-process DLL/bridge, if #31 or later phases justify one | — | Yes, if adopted | Conditionally yes | Must be authored and shipped by this repo, not a third-party framework; still subject to the read-only-first rule below |
| Official REDmod / CDPR modding surfaces (`.archive`, `tweakXL`-free vanilla asset overrides, the game's own `-modded` script cache mechanism) | Being surveyed separately | Unknown per-surface | Deferred to #20 | These are first-party CDPR surfaces, not third-party frameworks, but each must still be checked individually for whether it requires anything beyond what ships with the base game |

## Read-only-first

Until a separate, explicit owner decision, no candidate in this repo may
perform a mutating action against the game (`WriteProcessMemory`,
input injection intended to change game state, save-file writes, etc.).
Every probe in #29/#30/#31 is read-only by construction; this is
enforced by their own acceptance criteria, not re-stated here as a
separate mechanism.

## Semantics stay with `my-lisp`

This boundary is about *where code runs and what gets installed*, not
about *what anything means*. No dependency in the table above — allowed
or forbidden — may carry Lisp semantics into C++/Rust/PowerShell. A
probe or bridge produces `game-observation/1` records
([`docs/observation-contract-v0.md`](observation-contract-v0.md)); it
never decides what a fact *means* or what action follows from it.

## Negative test

If, after removing every entry in this table marked **No**, our proof
artifact still requires something to exist in the Cyberpunk 2077
install directory beyond the base game plus our own artifacts (per the
"Allowed" column), this task has not been satisfied — no exceptions,
no "just for now."

Verified for the current repo state: `tools/memory-probe-v0.ps1`
(#29's mechanism skeleton) requires nothing beyond the base game process
existing and our own script; it opens no handle with write access and
installs no file into the game directory.

## Related documents

- [`docs/observation-contract-v0.md`](observation-contract-v0.md)
- [`docs/neuraldeck-cet-not-installed-2026-09-16.md`](neuraldeck-cet-not-installed-2026-09-16.md) — records the last time CET *was* installed, under the pre-vanilla-pivot architecture; superseded by this boundary going forward
- [`owner-decision-2026-09-16-pause-and-revert-to-vanilla.md`](owner-decision-2026-09-16-pause-and-revert-to-vanilla.md)
