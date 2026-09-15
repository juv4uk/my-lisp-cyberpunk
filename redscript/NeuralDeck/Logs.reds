// Since game 2.01, the engine's native logging functions are no longer
// exposed to Redscript automatically -- each mod must declare the ones it
// uses itself. This is the standard community declaration snippet.
native func Log(const text: script_ref<String>) -> Void
native func LogChannel(channel: CName, const text: script_ref<String>) -> Void
