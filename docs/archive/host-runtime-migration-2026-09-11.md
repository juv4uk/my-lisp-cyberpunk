> **ARCHIVED — non-normative.** Superseded-by: `host-runtime/MIGRATION.md`,
> `host-runtime/VENDOR.md`. Written before Phase C/D completed; "mirror
> until Phase D" below is stale — Phase D is done, `wsm-my-lisp/dll` was
> deleted (their commit `1e1549a`).

# Host runtime migration · 2026-09-11

Per wsm-my-lisp P0 #15: Rust host embed leaves the self-hosting repo.

**Destination:** `my-lisp-cyberpunk/host-runtime/`  
**Source (mirror until Phase D):** `wsm-my-lisp/dll/`

Adapter (`adapter/`) still loads `wsm_my_lisp_cyberpunk_dll.dll` by name;
only the *build home* moves. SysV self-hosting (`asm/nucleus.s`, harness)
stays in wsm-my-lisp forever.
