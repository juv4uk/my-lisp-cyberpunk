# Game verification runbook

These checks turn the common Cyberpunk mod failures into explicit tests:
wrong installation root, missing dependency, stale adapter DLL, Redscript not
being compiled, plugin load failure and a hotkey that never reaches the host.

They only read files and logs. They never start the game, copy payloads, clear
the Redscript cache or change any installation.

## Before launching the game

```powershell
.\tools\Test-LocalGameDeployment.ps1 `
  -GameDir 'D:\games\Cyberpunk 2077 v.2.31 (2020)\Cyberpunk 2077' `
  -RequireFreshAdapter
```

This validates the game root, RED4ext, Codeware, both my-lisp DLLs, dispatch
scenario and NeuralDeck source. `-RequireFreshAdapter` also compares the
installed adapter hash with the current Release build, so a source build can
never be mistaken for the game payload.

## After launching the game

```powershell
.\tools\Test-GameLaunchEvidence.ps1 `
  -GameDir 'D:\games\Cyberpunk 2077 v.2.31 (2020)\Cyberpunk 2077'
```

It requires evidence that RED4ext loaded this plugin, Codeware initialized,
Redscript compiled `NeuralDeckOverlay.reds`, and the my-lisp session became
ready.

After pressing F10 once in a loaded save, add `-RequireF10`. That test proves
the physical key reached the adapter. It deliberately does not claim the popup
was rendered: that still needs the UI receipt and a visible panel.

## Why these tests exist

The community troubleshooting path consistently starts with the log chain:
RED4ext startup, plugin load, Redscript compilation and then the individual
mod log. The tests make that sequence reproducible instead of relying on a
manual reading of several folders.
