# Host runtime migration · 2026-09-11

Per wsm-my-lisp P0 #15: Rust host embed leaves the self-hosting repo.

**Destination:** `my-lisp-cyberpunk/host-runtime/`  
**Source (mirror until Phase D):** `wsm-my-lisp/dll/`

Adapter (`adapter/`) still loads `wsm_my_lisp_cyberpunk_dll.dll` by name;
only the *build home* moves. SysV self-hosting (`asm/nucleus.s`, harness)
stays in wsm-my-lisp forever.
