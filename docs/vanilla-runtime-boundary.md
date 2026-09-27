# Vanilla runtime boundary

Status: implements [#19](https://github.com/juv4uk/my-lisp-cyberpunk/issues/19)
(`CP-VANILLA-BOUNDARY`) and the exact-artifact admission rule from
[#43](https://github.com/juv4uk/my-lisp-cyberpunk/issues/43)
(`CP-VANILLA-GUARD-HASH-1`), under the Phase A epic
[#27](https://github.com/juv4uk/my-lisp-cyberpunk/issues/27).

## The rule, precisely

**Runtime dependency** = anything that must exist inside the Cyberpunk
2077 install directory, or be loaded into `Cyberpunk2077.exe`'s address
space, for our integration to work at all.

**Build-time reference** = anything we read, copy data from, or consult
while building our own artifact, that the finished artifact does not
require the game to have installed.

The boundary test is mechanical, not a judgment call. The final portable
architecture owns exactly one mod-side runtime file at the vanilla entry
point:

```text
bin\x64\version.dll
```

That filename is shared historically with third-party loaders, so the
filename itself is never authority. The deployed file is admitted only when
its SHA-256 is byte-for-byte identical to a separate trusted bridge artifact
built by this repo in the same revision/workflow.

## Dependency table

| Dependency | Build-time reference? | Runtime dependency? | Allowed? | Reason |
| --- | --- | --- | --- | --- |
| RED4ext SDK headers/docs (`adapter/deps/red4ext.sdk`) | Yes | No | Yes, as reference only | Struct layouts and RTTI type names are useful while researching; the finished vanilla artifact must not require RED4ext |
| `cyberpunk2077_addresses.json` (RED4ext-maintained public address/hash database) | Yes | No | Yes, as reference only | May inform reverse engineering; never installed as a runtime dependency |
| CET (Cyber Engine Tweaks) | No | — | **No** | Forbidden third-party runtime framework |
| RED4ext loader (`RED4ext.dll`, `bin\x64\winmm.dll` proxy) | No | — | **No** | Forbidden third-party runtime framework |
| Codeware | No | — | **No** | Forbidden third-party runtime framework |
| ArchiveXL / other third-party framework loaders | No | — | **No** | Forbidden runtime framework |
| `my-lisp.exe` | Yes, as a build/test oracle | No in the one-DLL production layout | **No as a shipped game-side dependency** | PR #49 statically embeds the canonical `my-lisp-embed` boundary into our bridge; the final game directory does not need `my-lisp.exe` |
| `my_lisp_embed.dll` | Build input is allowed | No | **No as a shipped game-side dependency** | The production bridge links the canonical embed boundary statically |
| Our exact portable `bin\x64\version.dll` | Built here | Yes | **Yes, by exact identity only** | It is our one-file bridge. `tools/Test-VanillaBoundary.ps1` requires a separate trusted build artifact and matching SHA-256 |
| `bin\x64\version-original.dll` | No | No | **No** | Retired forwarding layout. The portable bridge resolves the genuine `%SystemRoot%\System32\version.dll` by absolute path |
| Official REDmod / CDPR first-party surfaces | Being surveyed separately | Surface-dependent | Deferred to #20 | First-party surfaces are not automatically forbidden, but they are not part of the one-DLL admission exception |

## Exact bridge identity gate

`tools/Test-VanillaBoundary.ps1` has two valid states:

```text
no bin\x64\version.dll
    -> clean vanilla baseline; allowed

bin\x64\version.dll present
    -> require -AdmittedBridge <trusted build-side version.dll>
    -> compute SHA-256 of both files
    -> hashes equal: our exact artifact; continue checking the rest of boundary
    -> hashes differ: foreign/tampered artifact; fail closed
```

The admitted artifact must be a **separate file** from the deployed game-side
copy. Pointing `-AdmittedBridge` back at the installed file is rejected, so an
unknown loader cannot self-whitelist merely by being named `version.dll`.

The Windows verification workflow records the staged bridge SHA-256 as
`CYBERPUNK_BRIDGE_SHA256`. The same staged file is first exercised by the
one-DLL load/forwarding witness from #49 and is then copied into a temporary
game-root and admitted by #43's boundary guard. This ties these claims to one
artifact identity in one CI run:

```text
built bridge
  -> dependency inspection
  -> real DLL load + absolute System32 forwarding witness
  -> persistent canonical my-lisp Session witness
  -> exact SHA-256 boundary admission
```

A repository-global hard-coded hash is intentionally not the authority: a new
legitimate build may produce a new artifact. The authority is the exact hash
of the trusted artifact produced by the current pinned build, compared against
the deployed copy before use.

## Read-only-first

Until a separate, explicit owner decision, no candidate in this repo may
perform a mutating action against the game (`WriteProcessMemory`, input
injection intended to change game state, save-file writes, etc.). Boundary
admission says only **which runtime artifact is ours**; it does not grant
permission to mutate game state.

## Semantics stay with `my-lisp`

This boundary is about *where code runs and what gets installed*, not about
*what anything means*. PowerShell does not acquire Lisp semantics from this
hash gate. The bridge remains mechanism; canonical meaning stays upstream in
`my-lisp`. CML remains an optional compiler/AOT layer and is not an authority
for this admission decision.

## Negative and uninstall witnesses

`tools/tests/VanillaBoundaryGuardTest.ps1` covers the boundary as mutations:
clean game directory, foreign `version.dll`, exact admitted bridge, one-byte
mutation, bridge plus RED4ext, stale `version-original.dll`, unproven
`version.dll`, and generic ASI-loader presence.

Uninstalling our one-file payload means removing the owned
`bin\x64\version.dll`; no adjacent original DLL must be restored because the
bridge never renames or replaces Windows' real System32 file. After removal,
the guard returns to the clean-vanilla baseline state.

## Related documents

- [`docs/observation-contract-v0.md`](observation-contract-v0.md)
- [`docs/neuraldeck-cet-not-installed-2026-09-16.md`](neuraldeck-cet-not-installed-2026-09-16.md) — records the last time CET *was* installed, under the pre-vanilla-pivot architecture; superseded by this boundary going forward
- [`owner-decision-2026-09-16-pause-and-revert-to-vanilla.md`](owner-decision-2026-09-16-pause-and-revert-to-vanilla.md)
