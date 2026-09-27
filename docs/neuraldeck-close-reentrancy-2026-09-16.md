# NeuralDeck: re-entrant Close() permanently wedged the popup — 2026-09-16

## Status

Root-caused and fixed (commit `c751706`), from live evidence, not
speculation. Supersedes the open questions in
[`neuraldeck-logchannel-verification-gap-2026-09-16.md`](neuraldeck-logchannel-verification-gap-2026-09-16.md):
CET was the correct missing piece
([`neuraldeck-cet-not-installed-2026-09-16.md`](neuraldeck-cet-not-installed-2026-09-16.md)),
and with CET installed the real remaining bug became directly observable.

## Evidence

CET was installed (`cet_1.37.1`, official `maximegmd/CyberEngineTweaks`
release) and the owner pressed End repeatedly in a live session. Full
excerpts are attached in `docs/evidence/`:

- [`neuraldeck-cet-scripting-log-2026-09-16.txt`](evidence/neuraldeck-cet-scripting-log-2026-09-16.txt)
  (from `bin/x64/plugins/cyber_engine_tweaks/scripting.log`)
- [`neuraldeck-cet-gamelog-2026-09-16.txt`](evidence/neuraldeck-cet-gamelog-2026-09-16.txt)
  (from `bin/x64/plugins/cyber_engine_tweaks/gamelog.log`)

The decisive fragment:

```
05:41:52  NeuralDeck received Codeware End press
05:41:52  NeuralDeck ToggleOverlay showed popup
05:41:53  NeuralDeck received Codeware End press
05:41:53  NeuralDeck ToggleOverlay closing existing overlay
05:41:54  NeuralDeck received Codeware End press
05:41:54  NeuralDeck ToggleOverlay closing existing overlay
05:42:41  NeuralDeck received Codeware End press
05:42:41  NeuralDeck ToggleOverlay closing existing overlay
... (repeats through 05:44:02, never "showed popup" again)
```

The owner's own report matched exactly: a brief flash was visible on the
very first press, then nothing on every press after.

## Root cause

Read Codeware's own `scripts/UI/Popups/CustomPopup.reds`
(`psiberx/cp2077-codeware`) directly:

```redscript
public func Close() {
    let uiSystem: ref<UISystem> = GameInstance.GetUISystem(this.GetGame());
    let hideEvent: ref<HideCustomPopupEvent> = HideCustomPopupEvent.Create(this);
    uiSystem.QueueEvent(hideEvent);
}
```

`Close()` only queues an event. Processing happens a frame later
(`PopupsManager.OnHideCustomPopup` -> `CustomPopupManager.HidePopup` ->
`popupController.Detach()` -> `OnDetach()`), which then plays a 0.25s
fade-out animation (`OnHide()`), and only that animation's `OnFinish`
callback (`OnHideFinish`) calls `OnHidden()` -- the callback our own
`NeuralDeckOverlay.OnHidden()` override uses to null
`NeuralDeckService.m_overlay`.

Pressing End again before that ~0.25s+ chain completes found
`m_overlay` still non-null, so `ToggleOverlay()` called `Close()` a
second time on a popup that was already mid-close. That re-entered
`Detach()` -> `OnDetach()` -> restarted the fade animation from
scratch, overwriting the previous animation's `OnFinish` registration.
Repeated rapid presses kept restarting the fade indefinitely, so
`OnHideFinish`/`OnHidden` never actually completed, and `m_overlay`
stayed permanently non-null. Every later press -- even a single,
deliberate one -- just called `Close()` on an already-closing phantom
that could never re-open, which is exactly what "nothing appears when
I press End" meant once the state was wedged.

## Fix

`redscript/NeuralDeck/NeuralDeckOverlay.reds`, commit `c751706`: added
an `m_closing: Bool` guard on `NeuralDeckService`. `ToggleOverlay()`
now ignores a press that arrives while a close is already in flight
instead of re-issuing `Close()`; `OnOverlayHidden()` (the real
completion signal) resets both `m_overlay` and `m_closing` together.

## Next step

Not code. Relaunch the game so the fixed `.reds` recompiles, then press
End exactly once (not repeatedly) and confirm the popup is visibly on
screen.

## Related documents

- [`neuraldeck-logchannel-verification-gap-2026-09-16.md`](neuraldeck-logchannel-verification-gap-2026-09-16.md)
- [`neuraldeck-cet-not-installed-2026-09-16.md`](neuraldeck-cet-not-installed-2026-09-16.md)
- [`neuraldeck-f10-retrospective.md`](neuraldeck-f10-retrospective.md)
