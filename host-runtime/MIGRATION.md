# Migration: wsm-my-lisp/dll → my-lisp-cyberpunk/host-runtime

| Phase | Status |
|-------|--------|
| A inventory + freeze in wsm-my-lisp | done |
| B this tree as destination | done |
| C adapter builds/loads only from this tree | **this commit** — sources committed, sync script removed, 55/55 tests green on Windows |
| D remove or stub wsm-my-lisp/dll | next — coordinate with wsm-my-lisp before deleting their copy |

## Dual-maintenance window

Closed as of this commit for source drift purposes: this tree is the sole
source now. `wsm-my-lisp/dll` is still physically present in that repo
(frozen to bugfix/security only per `wsm-my-lisp#15`) until Phase D removes
or stubs it there — do not fix bugs in that copy going forward, fix them
here.

## Adapter

Build this crate; place `wsm_my_lisp_cyberpunk_dll.dll` beside the RED4ext
plugin (unchanged load path). `adapter/` itself already only references the
DLL by its build output path — verify it points at
`host-runtime/target/.../wsm_my_lisp_cyberpunk_dll.dll`, not anywhere under
a sibling `wsm-my-lisp` checkout, before calling Phase C fully done.
