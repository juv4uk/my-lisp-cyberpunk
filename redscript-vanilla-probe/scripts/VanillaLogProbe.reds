// CP-VANILLA-ENTRYPOINT-SURVEY (#20) falsification experiment.
//
// The one open question Family 4 (official REDmod) still has after
// documentation research alone (docs/research/vanilla-entrypoints.md):
// does a bare `native func Log` declaration, compiled ONLY through
// REDmod with zero RED4ext, zero CET, and zero other mods present,
// produce any observable output anywhere on disk?
//
// This mod does nothing else. It hooks a vanilla class method that is
// guaranteed to fire once at game start with no player input required
// (@wrapMethod always calls wrappedMethod(), so vanilla behavior is
// unchanged either way), and calls Log() exactly once.
//
// After a play session, check (at minimum):
//   r6\logs\redscript_rCURRENT.log
//   r6\logs\ (any other file REDmod/the engine creates)
// for the exact string below. Its presence or absence answers the
// question either way -- this experiment is designed to be falsifiable
// in one direction or the other, not just "try it and see."

native func Log(const text: script_ref<String>) -> Void

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
    Log("VANILLA-PROBE-20260916: Log() called via REDmod-only compile, zero RED4ext, zero CET, zero other mods present");
    wrappedMethod();
}
