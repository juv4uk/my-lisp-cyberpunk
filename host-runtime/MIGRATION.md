# Migration: wsm-my-lisp/dll → my-lisp-cyberpunk/host-runtime

| Phase | Status |
|-------|--------|
| A inventory + freeze in wsm-my-lisp | done |
| B this tree as destination | **this commit** |
| C adapter builds/loads only from this tree | next |
| D remove or stub wsm-my-lisp/dll | after C green |

## Dual-maintenance window

Until Phase C: prefer fixes **here**. Mirror may lag.

## Adapter

Build this crate; place `wsm_my_lisp_cyberpunk_dll.dll` beside the RED4ext
plugin (unchanged load path).
