# Vanilla entry-point survey

Status: implements [#20](https://github.com/juv4uk/my-lisp-cyberpunk/issues/20)
(`CP-VANILLA-ENTRYPOINT-SURVEY`), a Phase A prerequisite of
[#27](https://github.com/juv4uk/my-lisp-cyberpunk/issues/27)
(`CP-VANILLA-EPIC`). Cross-reference: [`docs/vanilla-runtime-boundary.md`](../vanilla-runtime-boundary.md) (#19)
defines what "third-party framework" excludes; this document surveys
what remains.

Labeling discipline: **confirmed** = verified against this repo's own
code/tests/docs or an authoritative source read directly this session;
**plausible** = consistent with public documentation but not yet
verified against a live game by this repo; **unknown** = genuinely
open, needs an experiment.

## Family 1 — External memory sidecar

- **Real entry point**: our own external process opens a handle on the
  already-running `Cyberpunk2077.exe` via `OpenProcess`, reads its
  memory via `ReadProcessMemory`. No code runs inside the game process.
- **What must be placed in game directory**: nothing. The sidecar and
  `my-lisp.exe` live entirely outside the install.
- **Process intervention required**: none beyond a read-only handle.
- **Read-only-first possible**: yes, natively — this family cannot
  write without a second, distinct WinAPI call (`WriteProcessMemory`)
  that a read-only implementation simply never makes.
- **Crash/thread/ABI risk**: **confirmed low for the game process** — a
  bad read from our side returns an error to *us*, it cannot corrupt or
  crash the target process the way an injected DLL's bad pointer
  dereference can (this repo's own 2026-09-15
  [`neuraldeck-f10-crash-2026-09-15.md`](../neuraldeck-f10-crash-2026-09-15.md)
  incident was exactly the in-process-injection failure mode this
  family avoids by construction).
- **Version dependency**: high. REDengine resolves object properties
  through its own RTTI at runtime rather than fixed struct offsets
  (confirmed by reading RED4ext's own approach, which exists
  specifically because of this); a genuinely validated offset requires
  live RE per build.
- **Smallest falsifiable proof**: read one scalar (e.g.
  `player-health-ratio`) twice, across a state change, and show both
  reads land in `docs/observation-contract-v0.md`'s `valid` shape with
  differing `value` — #29's stated evidence requirement.
- **What remains after removing all third-party frameworks**: the
  entire mechanism remains functional — it never depended on
  CET/RED4ext/Codeware to begin with.
- **Status this session**: **confirmed** for the process-discovery,
  module-base-resolve, and fail-closed parts
  ([`tools/memory-probe-v0.ps1`](../../tools/memory-probe-v0.ps1),
  tested locally with the game not running → correct `unavailable`
  emission). The actual offset read is **unknown**, blocked on a live
  RE session (see #29's issue comment).

## Family 2 — Screen/pixel oracle

- **Real entry point**: OS-level screen/window capture (Windows
  Graphics Capture API or Desktop Duplication API) of the game's own
  window, read purely as pixels.
- **What must be placed in game directory**: nothing.
- **Process intervention required**: none — this family never opens a
  handle to the game process at all, only to its rendered output.
- **Read-only-first possible**: yes, unconditionally — there is no
  write path in this family even in principle, since it never touches
  process memory or game state.
- **Crash/thread/ABI risk**: **confirmed lowest of all families** — a
  capture API failure affects only our own process.
- **Version dependency**: tied to HUD *layout*, not internal memory
  layout — a UI/HUD visual redesign breaks it, an internal engine
  refactor with the same HUD does not. Different failure axis than
  Family 1, not simply "less" or "more" fragile.
- **Smallest falsifiable proof**: capture the HUD health readout,
  classify the digits/bar-fill twice across a real health change,
  compare to Family 1's simultaneous read for #32's cross-channel
  check.
- **What remains after removing all third-party frameworks**: fully
  functional — this is the family least connected to REDengine
  internals of the three.
- **Status**: **plausible**, not yet implemented (#30 tracks this).

## Family 3 — Our own minimal in-process bridge

- **Real entry point**: a small DLL of our own authorship, loaded into
  the game process by one of: (a) a `winmm.dll`/`version.dll`-style
  proxy DLL placed next to the executable (the same load-order trick
  RED4ext itself uses, but with our own code, not RED4ext's), or (b) a
  hook on a stable, well-documented surface such as
  `IDXGISwapChain::Present` rather than REDengine internals directly.
- **What must be placed in game directory**: our own DLL, nothing
  third-party.
- **Process intervention required**: yes — this is genuine code
  injection into the game's address space, by construction.
- **Read-only-first possible**: yes as a *policy* choice (our DLL can
  choose to only read), but the mechanism itself is strictly more
  powerful and more dangerous than Families 1–2, since a bug here runs
  inside the game process and can crash it directly — the same failure
  class already lived through in
  [`neuraldeck-f10-crash-2026-09-15.md`](../neuraldeck-f10-crash-2026-09-15.md)
  and
  [`neuraldeck-close-reentrancy-2026-09-16.md`](../neuraldeck-close-reentrancy-2026-09-16.md),
  just without RED4ext underneath this time.
- **Crash/thread/ABI risk**: **confirmed high** — this repo has direct,
  first-hand evidence from its own RED4ext-based adapter of how easily
  an in-process boundary produces exactly this class of failure, even
  with a mature SDK underneath. A from-scratch DLL with no SDK at all
  starts from a strictly worse position.
- **Version dependency**: `IDXGISwapChain::Present`'s vtable shape is
  far more stable across game patches than REDengine's internal RTTI
  layout, per the DX12 API's own stability contract — the entry hook
  itself should be one of the least version-sensitive parts of this
  whole survey, even though whatever it reaches *inside* the game
  afterward is not.
- **Smallest falsifiable proof**: hook fires once per frame, our DLL
  writes one heartbeat fact to shared memory or a file, `my-lisp.exe`
  reads it externally.
- **What remains after removing all third-party frameworks**: still
  functional in principle, but this is the family that most resembles
  what RED4ext itself is — the #19 boundary table allows it
  conditionally exactly because "we wrote it ourselves" is a real,
  meaningful distinction from "we installed someone else's loader,"
  even though the risk profile is closer to RED4ext's than to
  Families 1–2's.
- **Status**: **plausible**, not yet implemented (#31 tracks this).
  Given the crash history above, this family should be the last one
  attempted, not the first, regardless of what #33's evidence matrix
  eventually shows for the other two.

## Family 4 — Official REDmod (first-party, not surveyed before this document)

- **Real entry point**: `REDmod`, CDPR's own official command-line
  modding tool, bundled with the base game install (confirmed via
  `CDPR-Modding-Documentation/Cyberpunk-Modding-Docs`, read directly
  this session). Mods live at `<game>/mods/<name>/{archives,scripts,
  tweaks,customSounds}` and REDmod compiles and stages them as part of
  the game's own regular load, with no separate loader DLL.
- **What must be placed in game directory**: only our own mod folder
  under `mods/`, using tooling that ships with the base game.
- **Process intervention required**: unknown in the injection sense —
  REDmod-staged Redscript runs inside the game's own script VM, which
  is a form of intervention, but it is the game's own first-party
  scripting surface rather than a third-party native-code loader.
- **Read-only-first possible**: plausible — Redscript itself has no
  `ReadProcessMemory`-equivalent capability; whatever it can observe is
  bounded to the RTTI-exposed script API surface the base game already
  ships, which is inherently narrower and safer than raw memory access.
- **Crash/thread/ABI risk**: plausible-low for pure Redscript (the
  language is sandboxed, cannot directly corrupt native memory the way
  a native plugin can) — but this is not yet verified against a live
  build in this repo.
- **Version dependency**: plausible-low relative to Families 1/3 — CDPR
  maintains REDmod and Redscript compatibility as part of shipping the
  base game itself, not a community project reverse-engineering it.
- **Smallest falsifiable proof**: **researched further 2026-09-16,
  evidence now leans negative but is not fully closed.** Checked the
  two real-world examples of Redscript-side file I/O found on Nexus —
  [RedFileSystem](https://www.nexusmods.com/cyberpunk2077/mods/13378)
  and [RedLogger](https://www.nexusmods.com/cyberpunk2077/mods/31920)
  — and both are explicitly **RED4ext plugins**; RedLogger's own
  description is "no CET required" but still lists RED4ext as a hard
  requirement. Neither is evidence that bare Redscript (REDmod-only,
  zero RED4ext) can write a file or otherwise signal outward. Separately,
  this repo's own live evidence (`docs/neuraldeck-logchannel-verification-gap-2026-09-16.md`)
  showed that `LogChannel()` calls produced **no output anywhere
  observable** until CET was installed to hook that native function —
  consistent with, though not proof of, native `Log`/`LogChannel`
  having no vanilla output sink at all without something hooking it.
  No public source was found describing a genuinely CET/RED4ext-free
  file-write or IPC-out path from Redscript. **This family is not yet
  fully falsified** — the one remaining gap is that this is
  documentation research, not a direct experiment against a running
  game with zero mods and a REDmod-compiled script calling declared
  `Log()`. That single experiment (does a bare `native func Log`
  declaration + REDmod-only compile produce anything in any file on
  disk, with nothing else installed) is the cheapest remaining way to
  close this family definitively, and needs a live session. If it
  cannot get data out, this family is a dead end for *this* project
  regardless of how clean its in-game side is, no matter how
  attractive the rest of its profile looks.
- **What remains after removing all third-party frameworks**: by
  definition, everything — REDmod is not a third-party framework to
  begin with.
- **Status**: **unknown**, genuinely open, not previously surveyed in
  this repo. Worth one cheap falsification experiment specifically
  targeted at the open question above (does bare Redscript have any
  observable-outside-the-process channel) before investing further.

## Synthesis

At least three technically distinct families exist (four found here):
external memory read, screen/pixel observation, in-process authored
DLL, and official first-party REDmod scripting. Each has at least one
candidate proof that does not require any of the frameworks
[`vanilla-runtime-boundary.md`](../vanilla-runtime-boundary.md) forbids.

Not ranked by preference. By fewest unverified assumptions, in order:
Family 2 (screen) has the fewest — it never touches the process at
all. Family 1 (memory) has one real unresolved assumption (finding a
valid offset via live RE). Family 4 (REDmod) has one large, binary
unresolved assumption (can it get data out at all) that should be
tested cheaply before further investment. Family 3 (in-process DLL) has
the most and the highest-consequence risk, confirmed by this repo's own
incident history, and should be attempted last if at all.
