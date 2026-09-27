# REDmod falsification experiment result: no output channel found

Status: closes the open question in
[`docs/research/vanilla-entrypoints.md`](vanilla-entrypoints.md) Family 4
and [`docs/research/cml-native-probe-path.md`](cml-native-probe-path.md)'s
motivating question, with a real live result rather than documentation
inference.

## Setup

- Mod deployed via official `tools\redmod\bin\redMod.exe deploy`, the
  first-party CDPR tool — zero RED4ext, zero CET, zero other mods
  installed (verified: `tools/Test-VanillaBoundary.ps1` held before this
  run, per its own evidence file the same day).
- Mod: `redscript-vanilla-probe/scripts/VanillaLogProbe.reds`, deployed
  to `<game>/mods/observation-probe-test/` with a minimal `info.json`
  (REDmod requires one; not documented as a prerequisite until this
  session found it the hard way — deploy failed once with "Missing
  info.json!" before it was added).
- Declares `native func Log`, hooks `PlayerPuppet.OnGameAttached` via
  `@wrapMethod` (verified against real published mod examples before
  deploying, not guessed), calls `Log()` once with the unique string
  `VANILLA-PROBE-20260916`.

## Result

`REDmodLog.txt` confirms clean deployment (`[DEPLOY] Stage 2/10 - Script
Compilation` through `Stage 10/10 - Finalize`, no errors). The live
session's `redscript_rCURRENT.log` confirms the engine's own script
loader compiled and loaded the mod cleanly at game start
(`Compilation complete`, `Output successfully saved to
...\final.redscripts`). The game ran normally the whole session — no
crash, `CrashInfo.json` stayed empty, process stayed alive throughout.

**The string `VANILLA-PROBE-20260916` was searched for and not found**
in `r6/logs/` (all files), `REDmodLog.txt`, or
`%LOCALAPPDATA%\CD Projekt Red\Cyberpunk 2077\` after a full play
session that loaded into the world (the owner confirmed being in-game,
character loaded — `OnGameAttached` should have fired).

Full logs attached:
[`redmod-log-2026-09-16-vanilla-probe.txt`](../evidence/redmod-log-2026-09-16-vanilla-probe.txt),
[`redscript-log-2026-09-16-vanilla-probe.txt`](../evidence/redscript-log-2026-09-16-vanilla-probe.txt).

## Conclusion

Family 4 (official REDmod) is a **dead end for getting data out of the
game process**, at least via the native `Log()` function and at least
via any of the output locations checked here. This confirms, with a
live experiment rather than inference, what
`docs/research/vanilla-entrypoints.md` already suspected from
`RedFileSystem`/`RedLogger` both requiring RED4ext: bare Redscript
compiled only through REDmod has no confirmed outward channel.

This does not prove Redscript is *entirely* silent without a hook
consumer -- it is possible some other native function, some other
output location not checked here, or some other hook point behaves
differently. But for the specific, most-likely candidate (`Log()`, the
most standard logging primitive, at the most standard hook point) the
answer is a clean no. Family 4 should not receive further investment
without a specific new reason to reopen it.

## Updated synthesis (#20/#26)

Of the four families surveyed, MEMORY (#29) and SCREEN (#30) remain the
only ones with a plausible path to getting a live fact out of the game
process without a third-party framework. Family 3 (in-process DLL,
#31) remains technically capable but highest-risk, per this repo's own
crash history. Family 4 (REDmod) is now closed by direct evidence, not
just deprioritized.

## Related

- [`docs/research/vanilla-entrypoints.md`](vanilla-entrypoints.md) (#20)
- [`docs/vanilla-runtime-boundary.md`](../vanilla-runtime-boundary.md) (#19)
- [`redscript-vanilla-probe/scripts/VanillaLogProbe.reds`](../../redscript-vanilla-probe/scripts/VanillaLogProbe.reds)
